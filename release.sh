#!/usr/bin/env bash

version="1.0.0"
release_name="denv-$version""-source"

files=(
	LICENSE
	README.md
	denv.1
	denv.h
	main.c
)

tar -cf "$release_name".tar --files-from /dev/null

echo "$release_name".tar

for file in "${files[@]}"; do
	tar -rf "$release_name".tar $file
done

