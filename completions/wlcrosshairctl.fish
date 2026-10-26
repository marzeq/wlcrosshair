# fish completion for wlcrosshairctl
complete -c wlcrosshairctl -f
complete -c wlcrosshairctl -n '__fish_use_subcommand' -a toggle -d 'Toggle crosshair visibility'
complete -c wlcrosshairctl -n '__fish_use_subcommand' -a show -d 'Show crosshair'
complete -c wlcrosshairctl -n '__fish_use_subcommand' -a hide -d 'Hide crosshair'
complete -c wlcrosshairctl -n '__fish_use_subcommand' -a reload -d 'Reload config'
complete -c wlcrosshairctl -n '__fish_use_subcommand' -a quit -d 'Quit daemon'
