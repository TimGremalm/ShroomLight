#!/bin/bash
FILE=main/version.h
if ! test -f "$FILE"; then
	echo "$FILE dont't exist, create a new"
	echo "uint version = 0;" > "$FILE"
fi

sed -i -r 's/.* = ([0-9]+)/echo uint version = $((\1+1))";"/e' main/version.h
