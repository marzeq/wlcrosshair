#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "../vendor/utils.h"

extern const char* socket_path;
bool valid_command(utils_str_view command);

int connect_socket(void) {
  int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return -1;
  struct sockaddr_un address = {.sun_family = AF_UNIX};
  snprintf(address.sun_path, sizeof(address.sun_path), "%s", socket_path);
  if (connect(fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

int run_cli(int argc, char** argv) {
  if (argc < 2) {
    fputs("Usage: wlcrosshairctl <command>\n", stderr);
    return 1;
  }
  utils_str_view command = utils_str_view_from_cstr(argv[1]);
  if (!valid_command(command)) {
    fprintf(stderr, "Invalid command: invalid ipc command\n");
    return 1;
  }
  int fd = connect_socket();
  if (fd < 0) {
    fputs("Failed to connect to daemon - is it running?\n", stderr);
    return 1;
  }
  size_t length = command.count;
  char buffer[16];
  memcpy(buffer, command.data, length);
  buffer[length] = '\n';
  ssize_t sent = write(fd, buffer, length + 1);
  close(fd);
  return sent == (ssize_t)(length + 1) ? 0 : 1;
}
