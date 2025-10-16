# Handle command not found using command-not-found
command_not_found_handler() {
    if [[ -x /usr/bin/command-not-found ]]; then
        /usr/bin/command-not-found "$1"
    fi
    return 127
}
