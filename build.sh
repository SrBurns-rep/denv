#!/usr/bin/env bash

if [ "$1" = "debug" ]
then
	gcc main.c -o denv -lz -g -Og -fsanitize=address,undefined
	echo "debug mode"
else
	gcc main.c -o denv -lz -O2
	strip denv
fi
