#!/bin/bash
mkdir -p build
gcc -std=c89 -Os -Wall -Wextra -pedantic -Werror -ggdb \
 -o build/handmaiden src/handmaiden.c
