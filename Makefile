all: clean sdl-handmaiden

CFLAGS=-std=c89 -O3 -march=native -Wall -Wextra -pedantic -Werror -ggdb
LFLAGS=-lm

clean:
	rm -rf build
	mkdir -p build

handmaiden:
	gcc -c $(CFLAGS) \
		-o build/handmaiden.o \
		src/handmaiden.c

sdl-handmaiden: handmaiden
	gcc $(CFLAGS) \
		-o build/sdl-handmaiden \
		src/sdl_handmaiden.c \
		build/handmaiden.o \
		`sdl2-config --cflags --libs` \
		$(LFLAGS)
