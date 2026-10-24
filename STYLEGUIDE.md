# C style

- Use C23 (`-std=gnu23` for the GNU extensions used by `utils.h`).
- Indent with two spaces. Do not use tab characters in C source.
- Bind pointer stars to the type: `char* foo`, `const char* path`.
- Put opening braces on the same line as control statements and function declarations.
- Keep related expressions together; do not split long lines merely to meet an arbitrary width.
- Use the vendored `utils.h` for arenas and string views where they simplify ownership and parsing.
- Keep authored code in `main.c`, `daemon/daemon.c`, and `cli/cli.c`. `main.c` directly includes both implementation files and the generated Wayland protocol `.c` files as one translation unit.
- Use ordinary file-scope functions and variables in authored code; this single translation unit does not need `static` linkage qualifiers.
- Make `wlcrosshairctl` a symlink to the `wlcrosshair` binary. Dispatch from the executable name.
- Generated Wayland protocol files and vendored source retain their upstream formatting.
