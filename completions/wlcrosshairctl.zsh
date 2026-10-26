#compdef wlcrosshairctl

_wlcrosshairctl() {
    _arguments \
        '1:command:(toggle show hide reload quit)'
}

_wlcrosshairctl "$@"
