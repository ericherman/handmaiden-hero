#define ERIC_DEBUG 0
#define HANDMAIDEN_TRY_TO_MAKE_VALGRIND_HAPPY ERIC_DEBUG

/* memory allocation by mmap requires _GNU_SOURCE since it is linux specific */
#define _GNU_SOURCE
#include <sys/mman.h>
/* end mmap includes */

/* the Simple Directmedia Library requires the following includes */
#include <SDL.h>
/* end SDL */

#include <stdio.h>		/* fprintf */
#include <stdint.h>		/* unit8_t */

#define FIRST_SUPPORTING -1
#define NONE 0

#define internal static
/*
#define global_variable static
#define local_persistant static
*/

struct offscreen_buffer {
	SDL_Texture *texture;
	int width;
	int height;
	uint32_t *pixels;
	uint8_t bytes_per_pixel;
	size_t pixels_bytes_len;
	/* bytes in a row of pixel data, including padding */
	size_t pitch;
};

struct virtual_window {
	int height;
	int width;
	uint8_t bytes_per_pixel;
	uint32_t *pixels;
	size_t pixels_bytes_len;
	/* bytes in a row of pixel data, including padding */
	size_t pitch;
};

internal void fill_virtual(struct virtual_window virtual_win, uint8_t offset)
{
	int x, y;
	uint8_t red, blue;
	uint32_t foreground;

	for (y = 0; y < virtual_win.height; y++) {
		for (x = 0; x < virtual_win.width; x++) {
			red = x + offset;	/* uint8_t means % 256 */
			blue = y + offset;
			foreground = (((uint32_t) blue) << 16) + (uint32_t) red;
			*(virtual_win.pixels + (y * virtual_win.width) + x) =
			    foreground;
		}
	}
}

internal void resize_offscreen(SDL_Window *window, SDL_Renderer *renderer,
			       struct offscreen_buffer *offscreen)
{
	int height, width;

	SDL_GetWindowSize(window, &width, &height);
	if (width == offscreen->width && height == offscreen->height) {
		/* nothing to do */
		return;
	}
	if (ERIC_DEBUG) {
		fprintf(stderr, "was %d x %d, need %d x %d\n",
			offscreen->width, offscreen->height, width, height);
	}
	if (offscreen->texture) {
		SDL_DestroyTexture(offscreen->texture);
	}
	if (offscreen->pixels) {
		munmap(offscreen->pixels, offscreen->pixels_bytes_len);
	}
	offscreen->width = width;
	offscreen->height = height;
	offscreen->texture = SDL_CreateTexture(renderer,
					       SDL_PIXELFORMAT_ARGB8888,
					       SDL_TEXTUREACCESS_STREAMING,
					       offscreen->width,
					       offscreen->height);
	if (!offscreen->texture) {
		fprintf(stderr, "Could not alloc offscreen->texture\n");
		exit(EXIT_FAILURE);
	}
	offscreen->pixels_bytes_len =
	    offscreen->height * offscreen->width * offscreen->bytes_per_pixel;
	offscreen->pixels =
	    mmap(0, offscreen->pixels_bytes_len, PROT_READ | PROT_WRITE,
		 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (!offscreen->pixels) {
		fprintf(stderr,
			"Could not alloc offscreen->pixels (%d)\n",
			offscreen->pixels_bytes_len);
		exit(EXIT_FAILURE);
	}
	offscreen->pitch = offscreen->width * offscreen->bytes_per_pixel;
}

/* TODO only bother resize when we get a resize event? */
void redraw(SDL_Window *window, SDL_Renderer *renderer,
	    struct offscreen_buffer *offscreen,
	    struct virtual_window virtual_win)
{
	int width, height, x, y, vert_y, vert_x, offset;
	float x_ratio, y_ratio;
	uint32_t foreground;

	SDL_GetWindowSize(window, &width, &height);

	if (width != offscreen->width || height != offscreen->height) {
		resize_offscreen(window, renderer, offscreen);
	}

	y_ratio = virtual_win.height / (float)height;
	x_ratio = virtual_win.width / (float)width;

	for (y = 0; y < height; y++) {
		vert_y = (int)(y * y_ratio);
		for (x = 0; x < width; x++) {
			vert_x = (int)(x * x_ratio);
			offset = (vert_y * virtual_win.width) + vert_x;
			foreground = *(virtual_win.pixels + offset);
			*(offscreen->pixels + (width * y) + x) = foreground;
		}
	}

	if (SDL_UpdateTexture
	    (offscreen->texture, 0, offscreen->pixels, offscreen->pitch)) {
		fprintf(stderr, "Could not SDL_UpdateTexture\n");
		exit(EXIT_FAILURE);
	}
	SDL_RenderCopy(renderer, offscreen->texture, 0, 0);
	SDL_RenderPresent(renderer);
}

int main(int argc, char *argv[])
{
	char *title;
	SDL_Window *window;
	Uint32 win_id;
	Uint32 flags;
	struct virtual_window virtual_win;
	int x, y, width, height, shutdown;
	SDL_Renderer *renderer;
	SDL_Event event;
	uint8_t offset;
	struct offscreen_buffer offscreen;

	offscreen.texture = NULL;
	offscreen.width = 0;
	offscreen.height = 0;
	offscreen.bytes_per_pixel = sizeof(uint32_t);
	offscreen.pixels = NULL;
	offscreen.pixels_bytes_len = 0;
	offscreen.pitch = 0;

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		return 1;
	}

	title = (argc > 1) ? argv[1] : "Handmaiden Hero";
	x = SDL_WINDOWPOS_UNDEFINED;
	y = SDL_WINDOWPOS_UNDEFINED;
	height = 480;
	width = 640;

	virtual_win.height = height;
	virtual_win.width = width;
	virtual_win.bytes_per_pixel = sizeof(uint32_t);
	virtual_win.pixels_bytes_len =
	    virtual_win.height * virtual_win.width *
	    virtual_win.bytes_per_pixel;

	virtual_win.pixels =
	    mmap(0, virtual_win.pixels_bytes_len, PROT_READ | PROT_WRITE,
		 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (!virtual_win.pixels) {
		fprintf(stderr, "Could not malloc virtual_win->pixels\n");
		return 1;
	}
	virtual_win.pitch = virtual_win.width * virtual_win.bytes_per_pixel;

	offset = 0;
	fill_virtual(virtual_win, offset++);

	flags = SDL_WINDOW_RESIZABLE;
	window = SDL_CreateWindow(title, x, y, width, height, flags);
	if (!window) {
		fprintf(stderr, "Could not XCreateSimpleWindow\n");
		return 2;
	}
	win_id = SDL_GetWindowID(window);
	renderer = SDL_CreateRenderer(window, FIRST_SUPPORTING, NONE);
	if (!renderer) {
		return 3;
	}

	shutdown = 0;
	while (!shutdown) {
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				shutdown = 1;
				break;
			case SDL_WINDOWEVENT:
				if (event.window.windowID != win_id) {
					/* not our event? */
					break;
				}
				switch (event.window.event) {
				case SDL_WINDOWEVENT_NONE:
					/* (docs say never used) */
					break;
				case SDL_WINDOWEVENT_SHOWN:
					break;
				case SDL_WINDOWEVENT_HIDDEN:
					break;
				case SDL_WINDOWEVENT_EXPOSED:
					/* window should be redrawn */
					break;
				case SDL_WINDOWEVENT_MOVED:
					/* window moved to data1, data2 */
					break;
				case SDL_WINDOWEVENT_RESIZED:
					/* window resized to data1 x data2 */
					/* always preceded by */
					/* SDL_WINDOWEVENT_SIZE_CHANGED */
					break;
				case SDL_WINDOWEVENT_SIZE_CHANGED:
					/* either as a result of an API call */
					/* or through the system or user action */
					/* this event is followed by */
					/* SDL_WINDOWEVENT_RESIZED if the size was */
					/* changed by an external event, i.e. the */
					/* user or the window manager */
					break;
				case SDL_WINDOWEVENT_MINIMIZED:
					break;
				case SDL_WINDOWEVENT_MAXIMIZED:
					break;
				case SDL_WINDOWEVENT_RESTORED:
					break;
				case SDL_WINDOWEVENT_ENTER:
					/* window has gained mouse focus */
					break;
				case SDL_WINDOWEVENT_LEAVE:
					/* window has lost mouse focus */
					break;
				case SDL_WINDOWEVENT_FOCUS_GAINED:
					/* window has gained keyboard focus */
					break;
				case SDL_WINDOWEVENT_FOCUS_LOST:
					/* window has lost keyboard focus */
					break;
				case SDL_WINDOWEVENT_CLOSE:
					/* the window manager requests close */
					event.type = SDL_QUIT;
					SDL_PushEvent(&event);
					break;
				default:
					/* (how did we get here? */
					break;
				}
				break;
			default:
				break;
			}
		}
		fill_virtual(virtual_win, offset++);
		redraw(window, renderer, &offscreen, virtual_win);
	}

	/* we probably do not need to do these next steps */
	if (HANDMAIDEN_TRY_TO_MAKE_VALGRIND_HAPPY) {
		munmap(virtual_win.pixels, virtual_win.pixels_bytes_len);
		if (offscreen.texture) {
			SDL_DestroyTexture(offscreen.texture);
			munmap(offscreen.pixels, offscreen.pixels_bytes_len);
		}
		/* other stuff */
	}

	return 0;
}
