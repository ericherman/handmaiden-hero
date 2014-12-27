all: clean handmaiden

clean:
	rm -rf build
	mkdir -p build

handmaiden:
	gcc -std=c89 -O3 -march=native -Wall -Wextra -pedantic -Werror -ggdb \
		-o build/handmaiden src/sdl_handmaiden.c \
		`sdl2-config --cflags --libs` -lm
