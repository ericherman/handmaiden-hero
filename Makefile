all: clean sdl-handmaiden

ifdef DEBUG
CSTD_CFLAGS=-std=gnu11
DBUG_CFLAGS=-O0 -march=native -ggdb \
 -D_GNU_SOURCE \
 -DERIC_DEBUG=1 \
 -DDEBUG_LOG_AUDIO=1 \
 -DHANDMAIDEN_TRY_TO_MAKE_VALGRIND_HAPPY=1 \
 -rdynamic
else
CSTD_CFLAGS=-std=c89 -pedantic
DBUG_CFLAGS=-O3 -march=native -ggdb \
 -fomit-frame-pointer \
 -DNDEBUG=1
endif

WARN_CFLAGS=-Wall -Wextra -Werror

CFLAGS=$(CSTD_CFLAGS) \
 $(WARN_CFLAGS) \
 $(DBUG_CFLAGS) \
 -pipe

LFLAGS=-lm

SDL2_CFLAGS=`sdl2-config --cflags`
SDL2_LFLAGS=`sdl2-config --libs`

# extracted from https://github.com/torvalds/linux/blob/master/scripts/Lindent
LINDENT=indent -npro -kr -i8 -ts8 -sob -l80 -ss -ncs -cp1 -il0

clean:
	rm -rf build
	mkdir -p build

handmaiden: src/handmaiden.h src/handmaiden.c
	gcc -c $(CFLAGS) \
		-o build/handmaiden.o \
		src/handmaiden.c

sdl-handmaiden: handmaiden src/sdl_handmaiden.c
	gcc $(CFLAGS) \
		$(SDL2_CFLAGS) \
		-o build/sdl-handmaiden \
		src/sdl_handmaiden.c \
		build/handmaiden.o \
		$(SDL2_LFLAGS) \
		$(LFLAGS)

tidy:
	$(LINDENT) \
		-T SDL_Renderer -T SDL_Window -T SDL_Texture -T SDL_Rect \
		src/*.h src/*.c
