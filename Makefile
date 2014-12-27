all: clean sdl-handmaiden

CFLAGS=-std=c89 -O3 -march=native -Wall -Wextra -pedantic -Werror -ggdb

clean:
	rm -rf build
	mkdir -p build

sdl-handmaiden:
	gcc $(CFLAGS) \
		-o build/sdl-handmaiden \
		src/sdl_handmaiden.c \
		`sdl2-config --cflags --libs` \
		-lm
