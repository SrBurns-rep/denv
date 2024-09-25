#!/usr/bin/env bash

if [[ $1 -eq "debug" ]]
then
	gcc main.c -o denv -lz -g -Og -fsanitize=address,undefined
else
	gcc main.c -o denv -lz
fi
