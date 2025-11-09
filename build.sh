#!/usr/bin/env bash

set -ex

version=($(cat version | tr '.' ' '))
OS=$(uname -s)

if [ "$1" = "debug" ]
then
    case $OS in
        Linux)
        	cc main.c -o denv -lz -g -Og -fsanitize=address,undefined -Wall -DDENV_VERSION_A=${version[0]} -DDENV_VERSION_B=${version[1]} -DDENV_VERSION_C=${version[2]} -DDEBUG_ON
        ;;
        NetBSD)
            cc main.c -o denv -lz -lrt -g -Og -Wall -DDENV_VERSION_A=${version[0]} -DDENV_VERSION_B=${version[1]} -DDENV_VERSION_C=${version[2]} -DDEBUG_ON
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
        	cc main.c -o denv -lz -O2 -Wall -DDENV_VERSION_A=${version[0]} -DDENV_VERSION_B=${version[1]} -DDENV_VERSION_C=${version[2]}
        ;;
        NetBSD)
            cc main.c -o denv -lz -lrt -O2 -Wall -DDENV_VERSION_A=${version[0]} -DDENV_VERSION_B=${version[1]} -DDENV_VERSION_C=${version[2]}
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
