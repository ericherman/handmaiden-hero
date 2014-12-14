#!/bin/bash
rm -rf gebouwd
mkdir -p gebouwd

unset CFLAGS
unset CXXFLAGS

gcc -std=c89 -O3 -march=native -Wall -Wextra -pedantic -Werror -ggdb \
 -o gebouwd/handmaiden src/handmaiden.c \
 `sdl2-config --cflags --libs`

# gcc -std=c89 -O3 -march=native -Wall -Wextra -pedantic -Werror -ggdb \
#  -o gebouwd/handmaiden-xlib src/handmaiden-xlib.c \
#  -lX11
