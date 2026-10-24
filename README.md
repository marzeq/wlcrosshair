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

Create a configuration file at `~/.config/wlcrosshair/config.toml` to customize the crosshair appearance. Example configuration:

```toml
path = "~/.config/wlcrosshair/rotated.png" # REQUIRED: path to crosshair image

output = "DP-2" # REQUIRED: output name as named by your compositor

size = 24 # REQUIRED: size of the crosshair in pixels, will resize the image to size x size pixels
# OR if the image is not square, use width and height instead:
# width = 32 # width of the crosshair in pixels
# height = 16 # height of the crosshair in pixels

# opacity = 1.0 # OPTIONAL: 0.0 (invisible) to 1.0 (original image opacity); defaults to 1.0
```

Opacity multiplies the image's existing alpha, so transparent pixels stay transparent. The bundled sample crosshairs have fully opaque visible pixels; use `opacity` to make them translucent.

Run `wlcrosshair` in the background through a systemd user service or your compositor's autostart configuration.

Initially, the crosshair will not be visible. You can toggle its visibility using the following command:

```bash
wlcrosshairctl toggle
# or show/hide to force visibility
```

To update after a config change while the app is running issue:

```bash
wlcrosshairctl reload
```

To close the crosshair overlay, either kill the `wlcrosshair` process or use:

```bash
wlcrosshairctl quit
```

If you do not have a crosshair image, you can find some samples I made [here](./sample_crosshairs/) (they're all 24x24, so set `size` to a multiple of 24)
or use one of these online crosshair generators:

- [KovaaK's](https://kovaaks.com/kovaaks/crosshair-creator/) - download for your resolution and set `width` & `height` to that resolution. If you're in 4K, download the 1080p version but set `width = 3840` and `height = 2160`, it will be perfectly scaled 2x
- [EasyAim](https://easyaim.com/crosshair-generator) - download by pressing the download button in the bottom right of the preview and set `size = 1000`

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
