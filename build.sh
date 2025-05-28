#!/usr/bin/env bash

set -ex

version=($(cat version | tr '.' ' '))
OS=$(uname -s)

if [ "$1" = "debug" ]
then
    case $OS in
        Linux)
        	gcc main.c -o denv -lz -g -Og -fsanitize=address,undefined -Wall -Wextra -DDENV_VERSION_A=${version[0]} -DDENV_VERSION_B=${version[1]} -DDENV_VERSION_C=${version[2]} -DDEBUG_ON
        ;;
        NetBSD)
            gcc main.c -o denv -lz -lrt -g -Og -Wall -Wextra -DDENV_VERSION_A=${version[0]} -DDENV_VERSION_B=${version[1]} -DDENV_VERSION_C=${version[2]} -DDEBUG_ON
        ;;
        # FreeBSD)
        # ;;
        # OpenBSD)
        # ;;
        *)
            echo "OS unsupported!"
            exit -1
        ;;
    esac
	echo "debug mode"
else
    case $OS in
        Linux)
        	gcc main.c -o denv -lz -O2 -Wall -Wextra -DDENV_VERSION_A=${version[0]} -DDENV_VERSION_B=${version[1]} -DDENV_VERSION_C=${version[2]}
        ;;
        NetBSD)
            gcc main.c -o denv -lz -lrt -O2 -Wall -Wextra -DDENV_VERSION_A=${version[0]} -DDENV_VERSION_B=${version[1]} -DDENV_VERSION_C=${version[2]}
        ;;
        # FreeBSD)
        # ;;
        # OpenBSD)
        # ;;
        *)
            echo "OS unsupported!"
            exit -1
        ;;
    esac
	strip denv
fi

mkdir -p $HOME/.local/share/denv

# if [ "$1" = "debug" ]
# then
# 	gcc main.c -o denv -lz -g -Og -fsanitize=address,undefined -Wall -Wextra -DDENV_VERSION_A=${version[0]} -DDENV_VERSION_B=${version[1]} -DDENV_VERSION_C=${version[2]} -DDEBUG_ON
# 	echo "debug mode"
# else
# 	gcc main.c -o denv -lz -O2 -Wall -Wextra -DDENV_VERSION_A=${version[0]} -DDENV_VERSION_B=${version[1]} -DDENV_VERSION_C=${version[2]}
# 	strip denv
# fi
