# bash completion for wlcrosshairctl
_wlcrosshairctl() {
    local cur="${COMP_WORDS[COMP_CWORD]}"
    COMPREPLY=($(compgen -W "toggle show hide reload quit" -- "$cur"))
}
complete -F _wlcrosshairctl wlcrosshairctl
