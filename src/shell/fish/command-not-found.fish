function fish_command_not_found
    test (count $argv) -eq 1; or return 127
    /usr/bin/command-not-found $argv[1]
    return 127
end