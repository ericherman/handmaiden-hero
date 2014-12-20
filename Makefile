all: clean handmaiden

clean:
	rm -rf gebouwd
	mkdir -p gebouwd

handmaiden:
	gcc -std=c89 -O3 -march=native -Wall -Wextra -pedantic -Werror -ggdb \
		-o gebouwd/handmaiden src/handmaiden.c \
		`sdl2-config --cflags --libs`
