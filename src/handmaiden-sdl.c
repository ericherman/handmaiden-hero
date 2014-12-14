#include <SDL.h>

int main(int argc, char *argv[])
{
	char *title;
	char *msg;
	SDL_Window *win;

	title = (argc > 1) ? argv[1] : "Handmaiden Hero";
	msg = (argc > 2) ? argv[2] : "Simple Directmedia Library";
	win = 0;

	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title, msg, win);
	return 0;
}
