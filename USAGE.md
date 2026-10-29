# Usage

## Configuration

Create a configuration file at `~/.config/wlcrosshair/config.toml` to customize the crosshair appearance. Example configuration:

```toml
path = "~/.config/wlcrosshair/rotated.png" # REQUIRED: path to crosshair image

# output = "DP-2" # OPTIONAL: select this output instead of the default

# size = 24 # OPTIONAL: resize to 24 x 24 pixels
# OR set width and/or height separately:
# width = 32 # OPTIONAL: defaults to the PNG's width
# height = 16 # OPTIONAL: defaults to the PNG's height

# opacity = 1.0 # OPTIONAL: 0.0 (invisible) to 1.0 (original image opacity); defaults to 1.0
```

Without `size`, `width` or `height`, the crosshair uses the PNG's actual dimensions. `size` sets both dimensions and takes precedence over `width` and `height`; otherwise, each omitted dimension uses the corresponding PNG dimension.

Without `output`, the only connected display is used on a one-monitor setup. With multiple displays, the name that sorts first alphabetically is used by default.

Opacity multiplies the image's existing alpha, so transparent pixels stay transparent.

### Finding your output name (when selecting a display)

Run the command for your compositor in a terminal inside your Wayland session. Look for names like `eDP-1`, `HDMI-A-1`, or `DP-2` in the output:

| Compositor | Command |
| --- | --- |
| KDE Plasma | `kscreen-doctor -o` |
| Hyprland | `hyprctl monitors` |
| Sway | `swaymsg -t get_outputs` |
| COSMIC | `cosmic-randr list` |
| niri | `niri msg outputs` |
| Other compositors (wlroots) | `wlr-randr` |
| Other compositors (non-wlroots) | `wayland-info` |

Put the name you found in `~/.config/wlcrosshair/config.toml`, for example `output = "eDP-1"`. Output names can change when you switch ports, docks or hardware; check again if the overlay cannot find the configured output.

## Starting and controlling the overlay

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

## Obtaining a crosshair image

We host an accompanying [generator website](https://marzeq.github.io/wlcrosshair/) where you can create a crosshair image and download it. There are also hand crafted sample crosshairs in `sample_crosshairs/` that you can use, they're 24x24 pixels.
