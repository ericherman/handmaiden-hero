#include <SDL.h>

int main(int argc, char *argv[])
{
	char *title;
	SDL_Window *win;
	int x, y, width, height;
	Uint32 flags;
	int shutdown;
	SDL_Event event;

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		return 1;
	}

	title = (argc > 1) ? argv[1] : "Handmaiden Hero";
	x = SDL_WINDOWPOS_UNDEFINED;
	y = SDL_WINDOWPOS_UNDEFINED;
	height = 480;
	width = 640;
	flags = SDL_WINDOW_RESIZABLE;

	win = SDL_CreateWindow(title,x,y,width, height,flags);
	if (!win) {
		return 2;
	}

	shutdown = 0;
	while (!shutdown) {
		SDL_WaitEvent(&event);
		switch(event.type) {
		case SDL_QUIT:
			shutdown = 1;
			break;
		case SDL_WINDOWEVENT:
			switch(event.window.event) {
			case SDL_WINDOWEVENT_RESIZED:
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}
	return 0;
}
