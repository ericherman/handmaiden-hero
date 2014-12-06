#!/bin/bash
rm -rf gebouwd
mkdir -p gebouwd

unset CFLAGS
unset CXXFLAGS

gcc -std=c89 -Os -Wall -Wextra -pedantic -Werror -ggdb \
 -o gebouwd/handmaiden src/handmaiden.c \
 -lX11

gcc -std=c89 -Os -Wall -Wextra -pedantic -Werror -ggdb \
 -o gebouwd/x11-example src/x11-example.c \
 -lX11
