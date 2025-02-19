#!/usr/bin/env bash

set -ex

version=($(cat version | tr '.' ' '))

if [ "$1" = "debug" ]
then
	gcc main.c -o denv -lz -g -Og -fsanitize=address,undefined -Wall -Wextra -DDENV_VERSION_A=${version[0]} -DDENV_VERSION_B=${version[1]} -DDENV_VERSION_C=${version[2]} -DDEBUG_ON
	echo "debug mode"
else
	gcc main.c -o denv -lz -O2 -Wall -Wextra -DDENV_VERSION_A=${version[0]} -DDENV_VERSION_B=${version[1]} -DDENV_VERSION_C=${version[2]}
	strip denv
fi
