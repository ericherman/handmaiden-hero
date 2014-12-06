#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdio.h>		/* fprintf */

#define BUF_LEN 255

void redraw(Display * display, Window window, GC context,
	    unsigned long foreground)
{
	XWindowAttributes window_attributes;
	int x, y;
	XGetWindowAttributes(display, window, &window_attributes);
	for (x = 0; x < window_attributes.width; x++) {
		for (y = 0; y < window_attributes.height; y++) {
			XSetForeground(display, context, foreground);
			XDrawPoint(display, window, context, x, y);
		}
	}
}

int main(int argc, char *argv[])
{
	char *display_name, buf[BUF_LEN];
	Display *display;
	Window parent;
	int screen, x, y, shutdown, len;
	unsigned int width, height, border_width, border;
	unsigned long background, foreground, white, black;
	Window window;
	Bool only_if_exists;
	Atom WM_DELETE_WINDOW;
	XEvent event;
	KeySym keysym;
	GC context;
	unsigned long valuemask;
	XGCValues *values;

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
	white = WhitePixel(display, screen);
	black = BlackPixel(display, screen);
	border = black;
	background = white;
	foreground = black;

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

	only_if_exists = False;
	WM_DELETE_WINDOW =
	    XInternAtom(display, "WM_DELETE_WINDOW", only_if_exists);
	XSetWMProtocols(display, window, &WM_DELETE_WINDOW, 1);

	valuemask = 0;
	values = NULL;
	context = XCreateGC(display, window, valuemask, values);

	shutdown = 0;
	while (!shutdown) {
		XNextEvent(display, &event);
		switch (event.type) {
		case Expose:
			foreground = (foreground == white) ? black : white;
			redraw(display, window, context, foreground);
			break;
		case KeyPress:
			buf[0] = '\0';
			len =
			    XLookupString(&event.xkey, buf, BUF_LEN, &keysym,
					  NULL);
			if (keysym == XK_Escape) {
				shutdown = 1;
			}
			if (len == 0) {
				fprintf(stderr, "Zero length keypress.\n");
			}
			break;
		case ClientMessage:
			if ((unsigned int)(event.xclient.data.l[0]) ==
			    (unsigned int)WM_DELETE_WINDOW) {
				shutdown = 1;
			}
			break;
		default:
			fprintf(stdout, "got event.type %d\n", event.type);
			break;
		}
	}

	/* we probably do not need to do these next steps */
	XDestroyWindow(display, window);
	XCloseDisplay(display);

	return 0;
}
