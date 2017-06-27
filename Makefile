all: clean sdl-handmaiden

CFLAGS=-std=c89 -Wall -Wextra -pedantic -Werror \
 -ggdb -O3 -march=native -fomit-frame-pointer \
 -pipe

LFLAGS=-lm

# extracted from https://github.com/torvalds/linux/blob/master/scripts/Lindent
LINDENT=indent -npro -kr -i8 -ts8 -sob -l80 -ss -ncs -cp1 -il0

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

tidy:
	$(LINDENT) \
		-T SDL_Renderer -T SDL_Window -T SDL_Texture -T SDL_Rect \
		src/*.h src/*.c
