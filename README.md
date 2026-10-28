# wlcrosshair

Simple crosshair overlay for Wayland. 

## Supported compositors

Any compositor that implements the layer shell protocol should work, including:

- **Hyprland**
- wlroots based compositors like **Sway**
- Smithay based compositors like **COSMIC** & **niri**
- **KDE Plasma** Wayland

Notable compositors that do not implement the layer shell protocol and thus are not supported:

- **GNOME**

Of course, as this is based on Wayland technology, it will not work on X11.

## Usage

See [USAGE.md](USAGE.md) for configuration and commands.

## Installation

### Building from source:

```bash
git clone git@github.com:marzeq/wlcrosshair.git
cd wlcrosshair

make
# optional: sudo make install PREFIX=/usr/local
```

`make` builds one executable, `wlcrosshair`, and creates `wlcrosshairctl` as a symlink. The executable selects daemon or CLI behavior from its name.

Build dependencies:

- C23 compiler (GCC, using `-std=gnu23`)
- GNU Make and pkg-config
- Wayland client and libpng development libraries

### Install from releases:

Grab the latest release binaries from the [releases page](https://github.com/marzeq/wlcrosshair/releases) and place them in your path.

### Install from package manager:

- AUR - [wlcrosshair](https://aur.archlinux.org/packages/wlcrosshair)

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## FAQ

#### Can you add a new feature?

No. I consider this project feature complete for my use case. Pull requests are welcome, but I will not be adding new features myself.

#### X11 support?

No. Let it die in peace.

#### GNOME support?

When they get their heads out of their asses and implement the protocol, of course.
