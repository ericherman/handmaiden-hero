all: sdl-handmaiden sdl-handmaiden-debug

DBUG_CFLAGS=-std=gnu11 \
 -Wall -Wextra -Werror \
 -O0 -march=native -ggdb \
 -D_GNU_SOURCE \
 -DERIC_DEBUG=1 \
 -DDEBUG_LOG_AUDIO=1 \
 -DHANDMAIDEN_TRY_TO_MAKE_VALGRIND_HAPPY=1 \
 -rdynamic \
 -pipe

CFLAGS=-std=c89 -pedantic \
 -Wall -Wextra -Werror \
 -O3 -march=native -ggdb \
 -fomit-frame-pointer \
 -DNDEBUG=1 \
 -pipe

LDADD=-lm

SDL2_CFLAGS=`sdl2-config --cflags`
SDL2_LIBS=`sdl2-config --libs`

# extracted from https://github.com/torvalds/linux/blob/master/scripts/Lindent
LINDENT=indent -npro -kr -i8 -ts8 -sob -l80 -ss -ncs -cp1 -il0

clean:
	rm -rf build
	mkdir -p build

sdl-handmaiden: clean \
		src/handmaiden.h src/handmaiden.c src/sdl_handmaiden.c
	gcc $(CFLAGS) \
		$(SDL2_CFLAGS) \
		-o build/sdl-handmaiden \
		src/handmaiden.c \
		src/sdl_handmaiden.c \
		$(SDL2_LIBS) \
		$(LDADD)

sdl-handmaiden-debug: clean \
		src/handmaiden.h src/handmaiden.c src/sdl_handmaiden.c
	gcc $(DBUG_CFLAGS) \
		$(SDL2_CFLAGS) \
		-o build/sdl-handmaiden \
		src/handmaiden.c \
		src/sdl_handmaiden.c \
		$(SDL2_LIBS) \
		$(LDADD)

tidy:
	$(LINDENT) \
		-T SDL_Renderer -T SDL_Window -T SDL_Texture -T SDL_Rect \
		src/*.h src/*.c
