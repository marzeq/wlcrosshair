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

### Configuration

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
Scaling averages covered source pixels, smoothing non-integer sizes while keeping exact integer enlargement crisp.

#### Finding your output name

Run the command for your compositor in a terminal inside your Wayland session. Find the display where you want the crosshair and copy its **output name** (for example, `DP-1`, `eDP-1` or `HDMI-A-1`):

| Compositor | Command | Name to copy |
| --- | --- | --- |
| KDE Plasma | `kscreen-doctor -o` | In `Output: 1 eDP-1 ...`, use `eDP-1`, not the numeric ID `1`. |
| Hyprland | `hyprctl monitors` | In `Monitor eDP-1 (ID 0):`, use `eDP-1`, not `0` or the `description:` value. |
| Sway | `swaymsg -t get_outputs` | Use the output's `name` value, such as `HDMI-A-1`, not its make or model. |
| COSMIC | `cosmic-randr list` | Use the name at the start of a display entry, such as `eDP-1 (enabled)`. |
| Other wlroots-based compositors (such as river, Wayfire and labwc) | `wlr-randr` | Use the name at the start of an output entry, before the quoted description, such as `DP-1 "Dell ..."`. This requires the compositor to provide the wlr-output-management protocol. |
| niri | `niri msg outputs` | Use the connector name, such as `DP-1` in `Output "Dell ..." (DP-1)`, not the quoted monitor description. |

For another Wayland compositor, check its own display or output listing command. `wayland-info` (from `wayland-utils`) can help if it reports an output `name` and the compositor advertises `zxdg_output_manager_v1`; use that name, not the monitor's make, model or description.

Put the name you found in `~/.config/wlcrosshair/config.toml`, for example `output = "eDP-1"`. Output names can change when you switch ports, docks or hardware; check again if the overlay cannot find the configured output.

### Starting and controlling the overlay

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
