#define HANDMAIDEN_TRY_TO_MAKE_VALGRIND_HAPPY 0

/* memory allocation by mmap requires _GNU_SOURCE since it is linux specific */
#define _GNU_SOURCE
#include <sys/mman.h>
/* end mmap includes */

/* the X11 windowing system requires the following includes */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
/* end X11 */

#include <stdio.h>		/* fprintf */
#include <stdint.h>		/* unit8_t */

#define BUF_LEN 255

#define internal static
/*
#define global_variable static
#define local_persistant static
*/

struct offscreen_buffer {
	XImage *image;
	int width;
	int height;
};

struct virtual_window {
	int height;
	int width;
	uint8_t bytes_per_pixel;
	uint32_t *pixels;
	size_t pixels_bytes_len;
};

internal void fill_virtual(struct virtual_window virtual_win, int offset)
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

void redraw(Display *display, Window window, GC context,
	    struct offscreen_buffer offscreen,
	    struct virtual_window virtual_win)
{
	XWindowAttributes window_attributes;
	int width, height, x, y, vert_y, vert_x, offset;
	float x_ratio, y_ratio;
	unsigned int foreground;

	XGetWindowAttributes(display, window, &window_attributes);

	width = window_attributes.width;
	height = window_attributes.height;

	if (width != offscreen.width || height != offscreen.height) {
		if (offscreen.image) {
			XDestroyImage(offscreen.image);
		}
		offscreen.width = width;
		offscreen.height = height;
		offscreen.image =
		    XGetImage(display, window, 0, 0, width, height, AllPlanes,
			      XYPixmap);
	}

	y_ratio = virtual_win.height / (float)height;
	x_ratio = virtual_win.width / (float)width;

	for (y = 0; y < height; y++) {
		vert_y = (int)(y * y_ratio);
		for (x = 0; x < width; x++) {
			vert_x = (int)(x * x_ratio);
			offset = (vert_y * virtual_win.width) + vert_x;
			foreground = *(virtual_win.pixels + offset);
			XPutPixel(offscreen.image, x, y, foreground);
		}
	}

	XPutImage(display, window, context, offscreen.image, 0, 0, 0, 0, width,
		  height);

	/* fprintf(stderr, "redrawn\n"); */
}

int main(int argc, char *argv[])
{
	char *display_name, buf[BUF_LEN];
	Display *display;
	Window parent;
	XSetWindowAttributes window_attributes;
	struct virtual_window virtual_win;
	int screen, x, y, shutdown, len;
	unsigned int width, height, border_width, border, msg;
	unsigned long background, black;
	Window window;
	Bool only_if_exists;
	Atom WM_DELETE_WINDOW;
	GC context;
	unsigned long valuemask;
	XGCValues *values;
	XEvent event;
	KeySym keysym;
	uint8_t offset;
	struct offscreen_buffer offscreen;

	offscreen.image = NULL;
	offscreen.width = 0;
	offscreen.height = 0;

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
	width = 640;
	height = 480;
	border_width = 1;
	black = BlackPixel(display, screen);
	border = black;
	background = black;

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

	offset = 0;
	fill_virtual(virtual_win, offset++);

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
		if (XCheckTypedEvent(display, ClientMessage, &event)) {
			if ((unsigned int)(event.xclient.data.l[0]) ==
			    (unsigned int)WM_DELETE_WINDOW) {
				XSync(display, True);
				shutdown = 1;
				if (0) {
					fprintf(stderr, "WM_DELETE_WINDOW\n");
				}
				break;
			} else {
				msg = (unsigned int)(event.xclient.data.l[0]);
				fprintf(stdout, "got ClientMessage: %u\n", msg);
			}
		}
		if (XCheckTypedEvent(display, KeyPress, &event)) {
			buf[0] = '\0';
			len =
			    XLookupString(&event.xkey, buf, BUF_LEN, &keysym,
					  NULL);
			if (keysym == XK_Escape) {
				XSync(display, True);
				shutdown = 1;
			}
			if (len == 0) {
				fprintf(stderr, "Zero length keypress.\n");
			}
			buf[len] = '\0';
			if (keysym != XK_Escape) {
				fprintf(stderr, "keypress:'%s'\n", buf);
			}
			break;
		}
		while (XCheckWindowEvent
		       (display, window, ExposureMask, &event)) ;
		fill_virtual(virtual_win, offset++);
		redraw(display, window, context, offscreen, virtual_win);
	}

	/* we probably do not need to do these next steps */
	if (HANDMAIDEN_TRY_TO_MAKE_VALGRIND_HAPPY) {
		munmap(virtual_win.pixels, virtual_win.pixels_bytes_len);
		if (offscreen.image) {
			XDestroyImage(offscreen.image);
		}
		XFreeGC(display, context);
		XDestroyWindow(display, window);
		XCloseDisplay(display);
	}

	return 0;
}
