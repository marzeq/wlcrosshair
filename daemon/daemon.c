#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <linux/memfd.h>
#include <math.h>
#include <png.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <unistd.h>
#include <wayland-client.h>

#include "../vendor/utils.h"
#include "../protocols/generated/wlr-layer-shell-unstable-v1-client-protocol.h"
#include "../protocols/generated/xdg-output-unstable-v1-client-protocol.h"

extern const char* socket_path;
bool valid_command(utils_str_view command);

typedef struct {
  char* path;
  char* output;
  int width;
  int height;
  double opacity;
} Config;

typedef enum { CONFIG_STRING,
               CONFIG_INT,
               CONFIG_FLOAT } ConfigValueType;

typedef struct {
  ConfigValueType type;
  char* string;
  int64_t integer;
  double floating;
} ConfigValue;

bool config_comment_only(utils_str_view text) {
  text = utils_str_view_trim_left(text);
  return !text.count || text.data[0] == '#';
}

bool config_digits(utils_str_view text, size_t* position, char* normalized, size_t* length) {
  bool found = false;
  while (*position < text.count) {
    char ch = text.data[*position];
    if (ch == '_') {
      if (!found || *position + 1 == text.count || !isdigit((unsigned char)text.data[*position + 1]))
        return false;
      (*position)++;
      continue;
    }
    if (!isdigit((unsigned char)ch))
      break;
    normalized[(*length)++] = ch;
    found = true;
    (*position)++;
  }
  return found;
}

bool config_number(utils_str_view text, ConfigValue* result, utils_allocator alloc) {
  char* normalized = utils_a_alloc(alloc, text.count + 1);
  if (!normalized)
    return false;
  size_t position = 0, length = 0;
  if (text.data[position] == '+' || text.data[position] == '-')
    normalized[length++] = text.data[position++];
  size_t integer_start = length;
  if (!config_digits(text, &position, normalized, &length))
    return false;
  if (length - integer_start > 1 && normalized[integer_start] == '0')
    return false;
  bool floating = false;
  if (position < text.count && text.data[position] == '.') {
    floating = true;
    normalized[length++] = text.data[position++];
    if (!config_digits(text, &position, normalized, &length))
      return false;
  }
  if (position < text.count && (text.data[position] == 'e' || text.data[position] == 'E')) {
    floating = true;
    normalized[length++] = text.data[position++];
    if (position < text.count && (text.data[position] == '+' || text.data[position] == '-'))
      normalized[length++] = text.data[position++];
    if (!config_digits(text, &position, normalized, &length))
      return false;
  }
  if (position != text.count)
    return false;
  normalized[length] = '\0';
  char* end;
  errno = 0;
  if (floating) {
    result->type = CONFIG_FLOAT;
    result->floating = strtod(normalized, &end);
  } else {
    result->type = CONFIG_INT;
    result->integer = strtoll(normalized, &end, 10);
  }
  return !errno && end == normalized + length;
}

bool config_value(utils_str_view text, ConfigValue* result, utils_allocator alloc) {
  text = utils_str_view_trim_left(text);
  if (!text.count || text.data[0] == '#')
    return false;
  if (text.data[0] == '"' || text.data[0] == '\'') {
    char quote = text.data[0];
    char* string = utils_a_alloc(alloc, text.count);
    if (!string)
      return false;
    size_t length = 0;
    for (size_t i = 1; i < text.count; i++) {
      char ch = text.data[i];
      if (ch == quote) {
        if (!config_comment_only(utils_str_view_substr(text, i + 1, text.count)))
          return false;
        string[length] = '\0';
        result->type = CONFIG_STRING;
        result->string = string;
        return true;
      }
      if (quote == '"' && ch == '\\') {
        if (++i == text.count)
          return false;
        switch (text.data[i]) {
        case 'b':
          ch = '\b';
          break;
        case 't':
          ch = '\t';
          break;
        case 'n':
          ch = '\n';
          break;
        case 'f':
          ch = '\f';
          break;
        case 'r':
          ch = '\r';
          break;
        case '"':
          ch = '"';
          break;
        case '\\':
          ch = '\\';
          break;
        default:
          return false;
        }
      } else if ((unsigned char)ch < 0x20)
        return false;
      string[length++] = ch;
    }
    return false;
  }
  size_t length = 0;
  while (length < text.count && !isspace((unsigned char)text.data[length]) && text.data[length] != '#')
    length++;
  if (!length || !config_comment_only(utils_str_view_substr(text, length, text.count)))
    return false;
  return config_number(utils_str_view_substr(text, 0, length), result, alloc);
}

bool load_config(const char* filename, Config* config, utils_arena* arena) {
  FILE* file = fopen(filename, "r");
  if (!file) {
    fprintf(stderr, "Failed to load config: %s\n", strerror(errno));
    return false;
  }
  utils_allocator alloc = utils_make_allocator_from_arena(arena);
  Config parsed = {.opacity = 1.0};
  int64_t size = 0, width = 0, height = 0;
  unsigned seen = 0;
  char* line = NULL;
  size_t capacity = 0, line_number = 0;
  ssize_t length;
  bool ok = true;
  while ((length = getline(&line, &capacity, file)) >= 0) {
    line_number++;
    utils_str_view text = utils_str_view_trim(utils_str_view_make(line, (size_t)length));
    if (config_comment_only(text))
      continue;
    int equal = utils_str_view_find(text, '=');
    if (equal < 0) {
      ok = false;
      break;
    }
    utils_str_view key = utils_str_view_trim(utils_str_view_substr(text, 0, (size_t)equal));
    for (size_t i = 0; i < key.count; i++) {
      char ch = key.data[i];
      if (!isalnum((unsigned char)ch) && ch != '_' && ch != '-')
        ok = false;
    }
    if (!key.count || !ok)
      break;
    ConfigValue value;
    if (!config_value(utils_str_view_substr(text, (size_t)equal + 1, text.count), &value, alloc)) {
      ok = false;
      break;
    }
    unsigned bit = 0;
    if (utils_str_view_eq_cstr(key, "path")) {
      bit = 1;
      if (value.type == CONFIG_STRING)
        parsed.path = value.string;
      else
        ok = false;
    } else if (utils_str_view_eq_cstr(key, "output")) {
      bit = 2;
      if (value.type == CONFIG_STRING)
        parsed.output = value.string;
      else
        ok = false;
    } else if (utils_str_view_eq_cstr(key, "size")) {
      bit = 4;
      if (value.type == CONFIG_INT)
        size = value.integer;
      else
        ok = false;
    } else if (utils_str_view_eq_cstr(key, "width")) {
      bit = 8;
      if (value.type == CONFIG_INT)
        width = value.integer;
      else
        ok = false;
    } else if (utils_str_view_eq_cstr(key, "height")) {
      bit = 16;
      if (value.type == CONFIG_INT)
        height = value.integer;
      else
        ok = false;
    } else if (utils_str_view_eq_cstr(key, "opacity")) {
      bit = 32;
      if (value.type == CONFIG_FLOAT)
        parsed.opacity = value.floating;
      else if (value.type == CONFIG_INT)
        parsed.opacity = (double)value.integer;
      else
        ok = false;
    }
    if (!ok || (seen & bit)) {
      ok = false;
      break;
    }
    seen |= bit;
  }
  if (ferror(file))
    ok = false;
  free(line);
  fclose(file);
  if (!ok) {
    fprintf(stderr, "Failed to load config: invalid value at line %zu\n", line_number);
    return false;
  }
  if (!parsed.path || !*parsed.path) {
    fputs("Failed to load config: 'path' is required\n", stderr);
    return false;
  }
  if (!parsed.output || !*parsed.output) {
    fputs("Failed to load config: 'output' is required\n", stderr);
    return false;
  }
  if (!isfinite(parsed.opacity) || parsed.opacity < 0.0 || parsed.opacity > 1.0) {
    fputs("Failed to load config: 'opacity' must be between 0.0 and 1.0\n", stderr);
    return false;
  }
  if (size > 0)
    width = height = size;
  if (width <= 0 || height <= 0 || width > INT_MAX || height > INT_MAX) {
    fputs("Failed to load config: either 'size' or both 'width' and 'height' are required\n", stderr);
    return false;
  }
  parsed.width = (int)width;
  parsed.height = (int)height;
  *config = parsed;
  return true;
}

struct wl_display* display;
struct wl_compositor* compositor;
struct wl_shm* shm;
struct zwlr_layer_shell_v1* layer_shell;
struct zxdg_output_manager_v1* xdg_output_manager;

struct wl_surface* surface;
struct zwlr_layer_surface_v1* layer_surface;
struct wl_buffer* buffer;

struct wl_output* target_output;

char requested_output[64];
int configured;

int surf_w, surf_h;

int png_w, png_h;
unsigned char* img_rgba;
unsigned char* scaled_pixels;
int scaled_w, scaled_h;

int shm_fd = -1;
void* shm_data = NULL;
size_t shm_size;

int create_shm(size_t size) {
  int fd = syscall(SYS_memfd_create, "wlcrosshair", MFD_CLOEXEC);
  if (fd < 0)
    return -1;
  if (ftruncate(fd, size) < 0)
    return -1;
  return fd;
}

void reset_state(void) {
  compositor = NULL;
  shm = NULL;
  layer_shell = NULL;
  xdg_output_manager = NULL;

  surface = NULL;
  layer_surface = NULL;
  buffer = NULL;

  target_output = NULL;

  shm_fd = -1;
  shm_data = NULL;
  shm_size = 0;

  configured = 0;
}

void premultiply(uint8_t* data, int w, int h, double opacity) {
  for (int i = 0; i < w * h; i++) {
    uint8_t r = data[0];
    uint8_t g = data[1];
    uint8_t b = data[2];
    uint8_t a = (uint8_t)(data[3] * opacity + 0.5);

    r = (r * a) / 255;
    g = (g * a) / 255;
    b = (b * a) / 255;

    data[0] = b;
    data[1] = g;
    data[2] = r;
    data[3] = a;

    data += 4;
  }
}

void free_scaled_png(void) {
  free(scaled_pixels);
  scaled_pixels = NULL;
  scaled_w = scaled_h = 0;
}

bool scale_png(void) {
  if (!img_rgba || png_w <= 0 || png_h <= 0 || surf_w <= 0 || surf_h <= 0) {
    free_scaled_png();
    return false;
  }
  size_t count = (size_t)surf_w * (size_t)surf_h;
  if (count > SIZE_MAX / 4) {
    free_scaled_png();
    return false;
  }
  unsigned char* pixels = malloc(count * 4);
  if (!pixels) {
    free_scaled_png();
    return false;
  }

  for (int y = 0; y < surf_h; y++) {
    double top = (double)y * png_h / surf_h;
    double bottom = (double)(y + 1) * png_h / surf_h;
    for (int x = 0; x < surf_w; x++) {
      double left = (double)x * png_w / surf_w;
      double right = (double)(x + 1) * png_w / surf_w;
      double sum[4] = {0};
      for (int sy = (int)top; sy < (int)ceil(bottom); sy++) {
        double covered_y = fmin(bottom, sy + 1.0) - fmax(top, (double)sy);
        for (int sx = (int)left; sx < (int)ceil(right); sx++) {
          double weight = covered_y * (fmin(right, sx + 1.0) - fmax(left, (double)sx));
          const unsigned char* source = img_rgba + ((size_t)sy * png_w + sx) * 4;
          for (int channel = 0; channel < 4; channel++)
            sum[channel] += source[channel] * weight;
        }
      }
      unsigned char* target = pixels + ((size_t)y * surf_w + x) * 4;
      double area = (right - left) * (bottom - top);
      for (int channel = 0; channel < 4; channel++)
        target[channel] = (unsigned char)(sum[channel] / area + 0.5);
    }
  }

  free(scaled_pixels);
  scaled_pixels = pixels;
  scaled_w = surf_w;
  scaled_h = surf_h;
  return true;
}

bool load_png(const char* path, double opacity) {
  FILE* fp = fopen(path, "rb");
  if (!fp)
    return false;

  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  png_infop info = png_create_info_struct(png);
  png_init_io(png, fp);

  png_read_info(png, info);

  png_w = png_get_image_width(png, info);
  png_h = png_get_image_height(png, info);

  png_set_expand(png);
  png_set_strip_16(png);
  png_set_gray_to_rgb(png);
  png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
  png_read_update_info(png, info);
  if (png_get_channels(png, info) != 4 || png_get_rowbytes(png, info) != (size_t)png_w * 4) {
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    return false;
  }

  free(img_rgba);
  img_rgba = malloc(png_w * png_h * 4);

  png_bytep rows[png_h];
  for (int y = 0; y < png_h; y++)
    rows[y] = img_rgba + y * png_w * 4;

  png_read_image(png, rows);
  png_destroy_read_struct(&png, &info, NULL);
  fclose(fp);

  premultiply(img_rgba, png_w, png_h, opacity);
  scale_png();
  return true;
}

void free_png(void) {
  free(img_rgba);
  img_rgba = NULL;
  free_scaled_png();
}

struct output_ctx {
  struct wl_output* out;
};

void xdg_position(void* data, struct zxdg_output_v1* xdg, int32_t x, int32_t y) {
  (void)data;
  (void)xdg;
  (void)x;
  (void)y;
}

void xdg_size(void* data, struct zxdg_output_v1* xdg, int32_t width, int32_t height) {
  (void)data;
  (void)xdg;
  (void)width;
  (void)height;
}

void xdg_done(void* data, struct zxdg_output_v1* xdg) {
  (void)data;
  (void)xdg;
}

void xdg_description(void* data, struct zxdg_output_v1* xdg, const char* description) {
  (void)data;
  (void)xdg;
  (void)description;
}

void xdg_name(void* data, struct zxdg_output_v1* xdg, const char* name) {
  (void)xdg;
  struct output_ctx* ctx = data;
  if (name && strcmp(name, requested_output) == 0)
    target_output = ctx->out;
}

const struct zxdg_output_v1_listener xdg_listener = {
    .logical_position = xdg_position,
    .logical_size = xdg_size,
    .done = xdg_done,
    .name = xdg_name,
    .description = xdg_description,
};

void alloc_shm(void) {
  if (shm_data) {
    munmap(shm_data, shm_size);
    shm_data = NULL;
  }
  if (shm_fd >= 0) {
    close(shm_fd);
    shm_fd = -1;
  }

  shm_size = surf_w * surf_h * 4;
  shm_fd = create_shm(shm_size);
  shm_data = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  memset(shm_data, 0, shm_size);
}

void create_buffer(void) {
  if (buffer) {
    wl_buffer_destroy(buffer);
    buffer = NULL;
  }

  struct wl_shm_pool* pool = wl_shm_create_pool(shm, shm_fd, shm_size);
  buffer = wl_shm_pool_create_buffer(pool, 0, surf_w, surf_h, surf_w * 4,
                                     WL_SHM_FORMAT_ARGB8888);
  wl_shm_pool_destroy(pool);
}

void clear_buffer(void) {
  memset(shm_data, 0, shm_size);
}

void blit_png(void) {
  memcpy(shm_data, scaled_pixels, shm_size);
}

void layer_configure(void* data,
                     struct zwlr_layer_surface_v1* ls,
                     uint32_t serial, uint32_t w, uint32_t h) {
  (void)data;
  zwlr_layer_surface_v1_ack_configure(ls, serial);

  if ((int)w != surf_w || (int)h != surf_h) {
    surf_w = w;
    surf_h = h;
    alloc_shm();
    create_buffer();
    if (img_rgba)
      scale_png();
  }

  configured = 1;
}

const struct zwlr_layer_surface_v1_listener layer_listener = {
    .configure = layer_configure,
    .closed = NULL};

void create_surface_internal(void) {
  surface = wl_compositor_create_surface(compositor);

  layer_surface = zwlr_layer_shell_v1_get_layer_surface(
      layer_shell,
      surface,
      target_output,
      ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
      "wlcrosshair");

  zwlr_layer_surface_v1_add_listener(layer_surface, &layer_listener, NULL);

  zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, -1);
  zwlr_layer_surface_v1_set_keyboard_interactivity(layer_surface, 0);
  zwlr_layer_surface_v1_set_size(layer_surface, surf_w, surf_h);
  zwlr_layer_surface_v1_set_anchor(layer_surface, 0);

  wl_surface_commit(surface);
}

void registry_add(void* data, struct wl_registry* reg,
                  uint32_t name, const char* iface, uint32_t ver) {
  (void)data;
  (void)ver;
  if (strcmp(iface, wl_compositor_interface.name) == 0)
    compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 4);

  else if (strcmp(iface, wl_shm_interface.name) == 0)
    shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);

  else if (strcmp(iface, zwlr_layer_shell_v1_interface.name) == 0)
    layer_shell = wl_registry_bind(reg, name, &zwlr_layer_shell_v1_interface, 4);

  else if (strcmp(iface, zxdg_output_manager_v1_interface.name) == 0)
    xdg_output_manager = wl_registry_bind(reg, name,
                                          &zxdg_output_manager_v1_interface, 3);

  else if (strcmp(iface, wl_output_interface.name) == 0) {
    struct wl_output* out =
        wl_registry_bind(reg, name, &wl_output_interface, 2);

    if (xdg_output_manager) {
      struct output_ctx* ctx = calloc(1, sizeof *ctx);
      ctx->out = out;

      struct zxdg_output_v1* xdg =
          zxdg_output_manager_v1_get_xdg_output(xdg_output_manager, out);

      zxdg_output_v1_add_listener(xdg, &xdg_listener, ctx);
    }
  }
}

const struct wl_registry_listener registry_listener = {
    .global = registry_add,
};

bool init_surface(int width, int height, const char* output_name) {
  if (display)
    wl_display_disconnect(display);

  display = wl_display_connect(NULL);
  if (!display)
    return false;

  reset_state();

  surf_w = width;
  surf_h = height;
  strncpy(requested_output, output_name, sizeof(requested_output) - 1);

  struct wl_registry* reg = wl_display_get_registry(display);
  wl_registry_add_listener(reg, &registry_listener, NULL);

  wl_display_roundtrip(display);
  wl_display_roundtrip(display);

  if (!target_output) {
    fprintf(stderr, "Could not find output '%s'\n", requested_output);
    return false;
  }

  alloc_shm();
  create_surface_internal();

  while (!configured)
    wl_display_roundtrip(display);

  create_buffer();
  clear_buffer();
  wl_surface_damage_buffer(surface, 0, 0, surf_w, surf_h);
  wl_surface_attach(surface, buffer, 0, 0);
  wl_surface_commit(surface);
  wl_display_flush(display);

  return true;
}

void destroy_surface(void) {
  free_scaled_png();
  if (buffer)
    wl_buffer_destroy(buffer);
  buffer = NULL;

  if (layer_surface)
    zwlr_layer_surface_v1_destroy(layer_surface);
  layer_surface = NULL;

  if (surface)
    wl_surface_destroy(surface);
  surface = NULL;

  if (shm_data)
    munmap(shm_data, shm_size);
  shm_data = NULL;

  if (shm_fd >= 0)
    close(shm_fd);
  shm_fd = -1;
}

void draw_png(void) {
  if (!img_rgba)
    return;
  if ((scaled_w != surf_w || scaled_h != surf_h || !scaled_pixels) && !scale_png())
    return;

  blit_png();
  wl_surface_damage_buffer(surface, 0, 0, surf_w, surf_h);
  wl_surface_attach(surface, buffer, 0, 0);
  wl_surface_commit(surface);
  wl_display_flush(display);
}

void clear_surface(void) {
  clear_buffer();
  wl_surface_damage_buffer(surface, 0, 0, surf_w, surf_h);
  wl_surface_attach(surface, buffer, 0, 0);
  wl_surface_commit(surface);
  wl_display_flush(display);
}

int listen_socket(void) {
  int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return -1;
  struct sockaddr_un address = {.sun_family = AF_UNIX};
  snprintf(address.sun_path, sizeof(address.sun_path), "%s", socket_path);
  unlink(socket_path);
  if (bind(fd, (struct sockaddr*)&address, sizeof(address)) < 0 || listen(fd, 16) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

utils_str_view receive_command(int fd, char buffer[256]) {
  struct timeval timeout = {.tv_sec = 1};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  size_t length = 0;
  while (length < 255) {
    ssize_t n = read(fd, buffer + length, 1);
    if (n <= 0 || buffer[length] == '\n')
      break;
    length++;
  }
  buffer[length] = '\0';
  return utils_str_view_trim(utils_str_view_make(buffer, length));
}

char* config_path(utils_allocator alloc) {
  const char* base = getenv("XDG_CONFIG_HOME");
  if (!base || !*base) {
    const char* home = getenv("HOME");
    if (!home || !*home)
      return NULL;
    size_t length = strlen(home) + sizeof("/.config/wlcrosshair/config.toml");
    char* path = utils_a_alloc(alloc, length);
    snprintf(path, length, "%s/.config/wlcrosshair/config.toml", home);
    return path;
  }
  size_t length = strlen(base) + sizeof("/wlcrosshair/config.toml");
  char* path = utils_a_alloc(alloc, length);
  snprintf(path, length, "%s/wlcrosshair/config.toml", base);
  return path;
}

char* expand_user(const char* path, utils_allocator alloc) {
  if (path[0] != '~' || (path[1] && path[1] != '/'))
    return (char*)path;
  const char* home = getenv("HOME");
  if (!home || !*home)
    return (char*)path;
  size_t length = strlen(home) + strlen(path + 1) + 1;
  char* expanded = utils_a_alloc(alloc, length);
  snprintf(expanded, length, "%s%s", home, path + 1);
  return expanded;
}

volatile sig_atomic_t stopping;
void stop_handler(int signal_number) {
  (void)signal_number;
  stopping = 1;
}

int run_daemon(void) {
  utils_arena config_arena = {0};
  utils_allocator config_alloc = utils_make_allocator_from_arena(&config_arena);
  char* filename = config_path(config_alloc);
  if (!filename) {
    fputs("Could not determine config path\n", stderr);
    return 1;
  }
  Config config;
  if (!load_config(filename, &config, &config_arena))
    return 1;
  if (!init_surface(config.width, config.height, config.output)) {
    fputs("failed to initalise surface\n", stderr);
    return 1;
  }
  load_png(expand_user(config.path, config_alloc), config.opacity);
  bool visible = false;
  int server = listen_socket();
  if (server < 0) {
    perror("Failed to listen on socket");
    destroy_surface();
    return 1;
  }
  signal(SIGINT, stop_handler);
  signal(SIGTERM, stop_handler);
  signal(SIGPIPE, SIG_IGN);
  int status = 0;
  while (!stopping) {
    struct pollfd fds[] = {{.fd = server, .events = POLLIN}, {.fd = wl_display_get_fd(display), .events = POLLIN}};
    if (wl_display_dispatch_pending(display) < 0 || wl_display_flush(display) < 0) {
      status = 1;
      break;
    }
    if (poll(fds, 2, -1) < 0) {
      if (errno == EINTR)
        continue;
      status = 1;
      break;
    }
    if (fds[1].revents & (POLLIN | POLLERR | POLLHUP)) {
      if (wl_display_dispatch(display) < 0) {
        status = 1;
        break;
      }
    }
    if (!(fds[0].revents & POLLIN))
      continue;
    int client = accept4(server, NULL, NULL, SOCK_CLOEXEC);
    if (client < 0)
      continue;
    char buffer[256];
    utils_str_view command = receive_command(client, buffer);
    close(client);
    if (!valid_command(command))
      continue;
    if (utils_str_view_eq_cstr(command, "quit"))
      break;
    if (utils_str_view_eq_cstr(command, "toggle")) {
      visible = !visible;
      if (visible)
        draw_png();
      else
        clear_surface();
    } else if (utils_str_view_eq_cstr(command, "show")) {
      draw_png();
      visible = true;
    } else if (utils_str_view_eq_cstr(command, "hide")) {
      clear_surface();
      visible = false;
    } else if (utils_str_view_eq_cstr(command, "reload")) {
      utils_arena next_arena = {0};
      utils_allocator next_alloc = utils_make_allocator_from_arena(&next_arena);
      Config next_config;
      if (!load_config(filename, &next_config, &next_arena)) {
        utils_a_reset(next_alloc);
        continue;
      }
      free_png();
      destroy_surface();
      if (!init_surface(next_config.width, next_config.height, next_config.output)) {
        fputs("failed to initalise surface\n", stderr);
        utils_a_reset(next_alloc);
        status = 1;
        break;
      }
      load_png(expand_user(next_config.path, next_alloc), next_config.opacity);
      if (visible)
        draw_png();
      utils_a_reset(config_alloc);
      config_arena = next_arena;
      config_alloc = utils_make_allocator_from_arena(&config_arena);
      filename = config_path(config_alloc);
    }
  }
  close(server);
  unlink(socket_path);
  free_png();
  destroy_surface();
  utils_a_reset(config_alloc);
  return status;
}
