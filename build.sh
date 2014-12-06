#!/bin/bash
rm -rf build
mkdir -p build

unset CFLAGS
unset CXXFLAGS

gcc -std=c89 -Os -Wall -Wextra -pedantic -Werror -ggdb \
 -o build/handmaiden src/handmaiden.c

gcc -std=c89 -Os -Wall -Wextra -pedantic -Werror -ggdb \
 -o build/x11-example src/x11-example.c \
 -lX11
