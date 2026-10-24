#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#define UTILS_IMPLEMENTATION
#include "vendor/utils.h"

#include <string.h>

const char* socket_path = "/tmp/crosshair.sock";

bool valid_command(utils_str_view command) {
  return utils_str_view_eq_cstr(command, "toggle") || utils_str_view_eq_cstr(command, "show") ||
         utils_str_view_eq_cstr(command, "hide") || utils_str_view_eq_cstr(command, "reload") ||
         utils_str_view_eq_cstr(command, "quit");
}

#include "vendor/tomlc17.c"
#include "protocols/generated/wlr-layer-shell-unstable-v1-client-protocol.c"
#include "protocols/generated/xdg-output-unstable-v1-client-protocol.c"
#include "protocols/generated/xdg-shell-client-protocol.c"
#include "daemon/daemon.c"
#include "cli/cli.c"

int main(int argc, char** argv) {
  const char* name = strrchr(argv[0], '/');
  name = name ? name + 1 : argv[0];
  return strcmp(name, "wlcrosshairctl") == 0 ? run_cli(argc, argv) : run_daemon();
}
