#include <SDL.h>

#define FIRST_SUPPORTING -1
#define NONE 0

#define internal static
#define local_persistent static

internal void handle_resize(SDL_Event * event)
{
	local_persistent int state;
	SDL_Window *win;
	SDL_Renderer *renderer;

	win = SDL_GetWindowFromID(event->window.windowID);
	renderer = SDL_GetRenderer(win);

	if (state) {
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	} else {
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	}
	state = !state;

	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);
}

int main(int argc, char *argv[])
{
	char *title;
	SDL_Window *win;
	Uint32 win_id;
	int x, y, width, height, shutdown;
	Uint32 flags;
	SDL_Renderer *renderer;
	SDL_Event event;

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		return 1;
	}

	title = (argc > 1) ? argv[1] : "Handmaiden Hero";
	x = SDL_WINDOWPOS_UNDEFINED;
	y = SDL_WINDOWPOS_UNDEFINED;
	height = 480;
	width = 640;
	flags = SDL_WINDOW_RESIZABLE;

	win = SDL_CreateWindow(title, x, y, width, height, flags);
	if (!win) {
		return 2;
	}
	win_id = SDL_GetWindowID(win);

	renderer = SDL_CreateRenderer(win, FIRST_SUPPORTING, NONE);
	if (!renderer) {
		return 3;
	}

	shutdown = 0;
	while (!shutdown) {
		SDL_WaitEvent(&event);
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
				handle_resize(&event);
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
	return 0;
}
