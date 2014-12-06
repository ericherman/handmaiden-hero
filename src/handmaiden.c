#include <X11/Xlib.h>

#include <stdio.h>		/* fprintf */
#include <unistd.h>		/* sleep */

int main(int argc, char *argv[])
{
	char *display_name;
	Display *display;
	Window parent;
	int screen, x, y;
	unsigned int width, height, border_width, border;
	unsigned long background;
	Window window;

	if (argc > 1) {
		display_name = argv[1];
	} else {
		display_name = NULL;
	}

	display = XOpenDisplay(display_name);
	if (!display) {
		fprintf(stderr, "Cannot open display '%s'\n", display_name);
		return 1;
	}
	screen = DefaultScreen(display);
	parent = RootWindow(display, screen);
	x = 0;
	y = 0;
	width = 300;
	height = 300;
	border_width = 1;
	border = BlackPixel(display, screen);
	background = WhitePixel(display, screen);

	window =
	    XCreateSimpleWindow(display, parent, x, y, width, height,
				border_width, border, background);

	if (!window) {
		fprintf(stderr, "Cannot XCreateSimpleWindow\n");
		return 1;
	}

	/* let us accept keypresses */
	XSelectInput(display, window, ExposureMask | KeyPressMask);

	/* the window becomes viewable */
	XMapWindow(display, window);

	XFlush(display);

	sleep(1);

	return 0;
}
