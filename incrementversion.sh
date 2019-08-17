#!/bin/bash
sed -i -r 's/.* = ([0-9]+)/echo uint version = $((\1+1))";"/e' main/version.h
