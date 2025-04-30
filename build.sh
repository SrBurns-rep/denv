#!/usr/bin/env bash

set -ex

version=($(cat version | tr '.' ' '))
OS=$(uname -s)

if [ "$1" = "debug" ]
    case $OS in
        Linux)
        	gcc main.c -o denv -lz -g -Og -fsanitize=address,undefined -Wall -Wextra -DDENV_VERSION_A=${version[0]} -DDENV_VERSION_B=${version[1]} -DDENV_VERSION_C=${version[2]} -DDEBUG_ON
        NetBSD)
        ;;
        FreeBSD
        ;;
        OpenBSD)
        ;;
        *)
            echo "OS unsupported!"
            exit -1
        ;;
    esac
	echo "debug mode"
then
    case $OS in
        Linux)
        	gcc main.c -o denv -lz -O2 -Wall -Wextra -DDENV_VERSION_A=${version[0]} -DDENV_VERSION_B=${version[1]} -DDENV_VERSION_C=${version[2]}
        ;;
        NetBSD)
        ;;
        FreeBSD
        ;;
        OpenBSD)
        ;;
        *)
            echo "OS unsupported!"
            exit -1
        ;;
    esac
	strip denv
fi

# if [ "$1" = "debug" ]
# then
# 	gcc main.c -o denv -lz -g -Og -fsanitize=address,undefined -Wall -Wextra -DDENV_VERSION_A=${version[0]} -DDENV_VERSION_B=${version[1]} -DDENV_VERSION_C=${version[2]} -DDEBUG_ON
# 	echo "debug mode"
# else
# 	gcc main.c -o denv -lz -O2 -Wall -Wextra -DDENV_VERSION_A=${version[0]} -DDENV_VERSION_B=${version[1]} -DDENV_VERSION_C=${version[2]}
# 	strip denv
# fi
