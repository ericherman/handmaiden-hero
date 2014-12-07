#define HANDMAIDEN_TRY_TO_MAKE_VALGRIND_HAPPY 0

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdio.h>		/* fprintf */
#include <stdlib.h>		/* malloc */

#define BUF_LEN 255

/*
#define internal_function static
#define global_variable static
#define local_persistant static
*/

struct virtual_window {
	int height;
	int width;
	unsigned int *pixels;
};

void redraw(Display *display, Window window, GC context,
	    struct virtual_window *virtual_win)
{
	XWindowAttributes window_attributes;
	int x, y, vert_y, vert_x, offset;
	float x_ratio, y_ratio;
	unsigned int foreground;

	XGetWindowAttributes(display, window, &window_attributes);

	y_ratio = virtual_win->height / (float)window_attributes.height;
	x_ratio = virtual_win->width / (float)window_attributes.width;

	for (y = 0; y < window_attributes.height; y++) {
		vert_y = (int)(y * y_ratio);
		for (x = 0; x < window_attributes.width; x++) {
			vert_x = (int)(x * x_ratio);
			offset = (vert_y * virtual_win->width) + vert_x;
			foreground = *(virtual_win->pixels + offset);
			XSetForeground(display, context, foreground);
			XDrawPoint(display, window, context, x, y);
		}
	}
	fprintf(stderr, "redrawn\n");
}

int main(int argc, char *argv[])
{
	char *display_name, buf[BUF_LEN];
	Display *display;
	Window parent;
	XSetWindowAttributes window_attributes;
	struct virtual_window *virtual_win;
	int screen, x, y, shutdown, len;
	unsigned int width, height, border_width, border, foreground;
	unsigned long background, black;
	Window window;
	Bool only_if_exists;
	Atom WM_DELETE_WINDOW;
	GC context;
	unsigned long valuemask;
	XGCValues *values;
	XEvent event;
	KeySym keysym;

	if (argc > 1) {
		display_name = argv[1];
	} else {
		display_name = NULL;
	}

	display = XOpenDisplay(display_name);
	if (!display) {
		fprintf(stderr, "Could not open display '%s'\n", display_name);
		return 1;
	}
	screen = DefaultScreen(display);
	parent = RootWindow(display, screen);
	x = 0;
	y = 0;
	width = 255;
	height = 255;
	border_width = 1;
	black = BlackPixel(display, screen);
	border = black;
	background = black;

	virtual_win = malloc(sizeof(struct virtual_window));
	if (!virtual_win) {
		fprintf(stderr, "Could not malloc virtual_window\n");
		return 1;
	}
	virtual_win->height = height;
	virtual_win->width = width;
	virtual_win->pixels =
	    malloc(virtual_win->height * virtual_win->width *
		   sizeof(unsigned int));
	if (!virtual_win->pixels) {
		fprintf(stderr, "Could not malloc virtual_win->pixels\n");
		return 1;
	}

	for (y = 0; y < virtual_win->height; y++) {
		for (x = 0; x < virtual_win->width; x++) {
			foreground = ((x % 256) << 16) + (y % 256);
			*(virtual_win->pixels + (y * virtual_win->width) + x) =
			    foreground;
		}
	}

	window =
	    XCreateSimpleWindow(display, parent, x, y, width, height,
				border_width, border, background);

	if (!window) {
		fprintf(stderr, "Could not XCreateSimpleWindow\n");
		return 1;
	}

	/* Set window attributes such that off display pixels are preserved. */
	window_attributes.backing_store = Always;
	window_attributes.backing_pixel = BlackPixel(display, screen);
	XChangeWindowAttributes(display, window,
				CWBackingStore | CWBackingPixel,
				&window_attributes);

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
			redraw(display, window, context, virtual_win);
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
			buf[len] = '\0';
			fprintf(stderr, "keypress:'%s'\n", buf);
			break;
		case ClientMessage:
			if ((unsigned int)(event.xclient.data.l[0]) ==
			    (unsigned int)WM_DELETE_WINDOW) {
				shutdown = 1;
				fprintf(stderr, "WM_DELETE_WINDOW\n");
			} else {
				fprintf(stdout, "got ClientMessage: %u\n",
					(unsigned int)(event.xclient.data.
						       l[0]));
			}
			break;
		default:
			fprintf(stdout, "got event.type %d\n", event.type);
			break;
		}
	}

	/* we probably do not need to do these next steps */
	if (HANDMAIDEN_TRY_TO_MAKE_VALGRIND_HAPPY) {
		free(virtual_win->pixels);
		free(virtual_win);
		XFreeGC(display, context);
		XDestroyWindow(display, window);
		XCloseDisplay(display);
	}

	return 0;
}
