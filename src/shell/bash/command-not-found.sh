command_not_found_handle() {
    [ $# -eq 1 ] || return 127
    /usr/bin/command-not-found "$1"
    return 127
}