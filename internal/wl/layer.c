#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "layer.h"

#include <wayland-client.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

#include <png.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <linux/memfd.h>
#include <stdio.h>
#include <stdbool.h>


static struct wl_display *display;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct zwlr_layer_shell_v1 *layer_shell;
static struct zxdg_output_manager_v1 *xdg_output_manager;

static struct wl_surface *surface;
static struct zwlr_layer_surface_v1 *layer_surface;
static struct wl_buffer *buffer;

static struct wl_output *target_output;

static char requested_output[64];
static int configured;

static int surf_w, surf_h;

static int png_w, png_h;
static unsigned char *img_rgba;

static int shm_fd = -1;
static void *shm_data = NULL;
static size_t shm_size;


static int create_shm(size_t size) {
  int fd = syscall(SYS_memfd_create, "wlcrosshair", MFD_CLOEXEC);
  if (fd < 0)
    return -1;
  if (ftruncate(fd, size) < 0)
    return -1;
  return fd;
}

static void reset_state(void) {
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



static void premultiply(uint8_t *data, int w, int h) {
  for (int i = 0; i < w * h; i++) {
    uint8_t r = data[0];
    uint8_t g = data[1];
    uint8_t b = data[2];
    uint8_t a = data[3];

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

bool load_png(const char *path) {
  FILE *fp = fopen(path, "rb");
  if (!fp) return false;

  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  png_infop info = png_create_info_struct(png);
  png_init_io(png, fp);

  png_read_info(png, info);

  png_w = png_get_image_width(png, info);
  png_h = png_get_image_height(png, info);

  png_set_expand(png);
  png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
  png_read_update_info(png, info);

  free(img_rgba);
  img_rgba = malloc(png_w * png_h * 4);

  png_bytep rows[png_h];
  for (int y = 0; y < png_h; y++)
    rows[y] = img_rgba + y * png_w * 4;

  png_read_image(png, rows);
  png_destroy_read_struct(&png, &info, NULL);
  fclose(fp);

  premultiply(img_rgba, png_w, png_h);
  return true;
}

void free_png(void) {
  free(img_rgba);
  img_rgba = NULL;
}


struct output_ctx {
  struct wl_output *out;
};

static void xdg_noop(void *a, void *b) {}

static void xdg_name(void *data, struct zxdg_output_v1 *xdg, const char *name) {
  struct output_ctx *ctx = data;
  if (name && strcmp(name, requested_output) == 0)
    target_output = ctx->out;
}

static const struct zxdg_output_v1_listener xdg_listener = {
  .logical_position = (void*)xdg_noop,
  .logical_size = (void*)xdg_noop,
  .done = (void*)xdg_noop,
  .name = xdg_name,
  .description = (void*)xdg_noop,
};


static void alloc_shm(void) {
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

static void create_buffer(void) {
  if (buffer) {
    wl_buffer_destroy(buffer);
    buffer = NULL;
  }

  struct wl_shm_pool *pool = wl_shm_create_pool(shm, shm_fd, shm_size);
  buffer = wl_shm_pool_create_buffer(pool, 0, surf_w, surf_h, surf_w * 4,
                     WL_SHM_FORMAT_ARGB8888);
  wl_shm_pool_destroy(pool);
}

static void clear_buffer(void) {
  memset(shm_data, 0, shm_size);
}


static void blit_png(void) {
  uint32_t *dst = shm_data;
  uint32_t *src = (uint32_t*)img_rgba;

  for (int y = 0; y < surf_h; y++) {
    int sy = y * png_h / surf_h;
    for (int x = 0; x < surf_w; x++) {
      int sx = x * png_w / surf_w;
      dst[y*surf_w + x] = src[sy*png_w + sx];
    }
  }
}


static void layer_configure(void *data,
              struct zwlr_layer_surface_v1 *ls,
              uint32_t serial, uint32_t w, uint32_t h) {
  zwlr_layer_surface_v1_ack_configure(ls, serial);

  if ((int)w != surf_w || (int)h != surf_h) {
    surf_w = w;
    surf_h = h;
    alloc_shm();
    create_buffer();
  }

  configured = 1;
}

static const struct zwlr_layer_surface_v1_listener layer_listener = {
  .configure = layer_configure,
  .closed = NULL
};

static void create_surface_internal(void) {
  surface = wl_compositor_create_surface(compositor);

  layer_surface = zwlr_layer_shell_v1_get_layer_surface(
    layer_shell,
    surface,
    target_output,
    ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
    "wlcrosshair"
  );

  zwlr_layer_surface_v1_add_listener(layer_surface, &layer_listener, NULL);

  zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, -1);
  zwlr_layer_surface_v1_set_keyboard_interactivity(layer_surface, 0);
  zwlr_layer_surface_v1_set_size(layer_surface, surf_w, surf_h);
  zwlr_layer_surface_v1_set_anchor(layer_surface, 0);

  wl_surface_commit(surface);
}


static void registry_add(void *data, struct wl_registry *reg,
             uint32_t name, const char *iface, uint32_t ver) {
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
    struct wl_output *out =
      wl_registry_bind(reg, name, &wl_output_interface, 2);

    if (xdg_output_manager) {
      struct output_ctx *ctx = calloc(1, sizeof *ctx);
      ctx->out = out;

      struct zxdg_output_v1 *xdg =
        zxdg_output_manager_v1_get_xdg_output(xdg_output_manager, out);

      zxdg_output_v1_add_listener(xdg, &xdg_listener, ctx);
    }
  }
}

static const struct wl_registry_listener registry_listener = {
  .global = registry_add,
};


bool init_surface(int width, int height, const char *output_name) {
  if (display)
    wl_display_disconnect(display);

  display = wl_display_connect(NULL);
  if (!display)
    return false;

  reset_state();

  surf_w = width;
  surf_h = height;
  strncpy(requested_output, output_name, sizeof(requested_output)-1);

  struct wl_registry *reg = wl_display_get_registry(display);
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
  if (!img_rgba) return;

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
