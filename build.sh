#!/bin/bash
rm -rf gebouwd
mkdir -p gebouwd

unset CFLAGS
unset CXXFLAGS

gcc -std=c89 -O3 -march=native -Wall -Wextra -pedantic -Werror -ggdb \
 -o gebouwd/handmaiden src/handmaiden.c \
 -lX11

gcc -std=c89 -O3 -march=native -Wall -Wextra -pedantic -Werror -ggdb \
 -o gebouwd/handmaiden-sdl src/handmaiden-sdl.c \
 `sdl2-config --cflags --libs`
