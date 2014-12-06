/**
 * (C)2012 Geeks3D.com
 *
 * code copy-pasted from:
 * http://www.geeks3d.com/20120102/programming-tutorial-simple-x11-x-window-code-sample-for-linux-and-mac-os-x/
 *
 * Posted by JeGX - 2012/01/02 at 16:41
 *
 * plus Changes:
 * Copyright (c) 2014 Eric Herman <eric@freesa.org>
 * 2014-12-06 Make code C89 compatible
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/utsname.h>

int main(int argc, char **argv)
{
	Display *dpy;
	int s, ret, y_offset, height, width, len;
	Window win;
	Atom WM_DELETE_WINDOW;
	unsigned char uname_ok;
	struct utsname sname;
	XEvent e;
	char *s1, *s2, buf[256];
	KeySym keysym;

	dpy = XOpenDisplay(NULL);
	if (dpy == NULL) {
		fprintf(stderr, "Cannot open display\n");
		exit(1);
	}

	s = DefaultScreen(dpy);
	win =
	    XCreateSimpleWindow(dpy, RootWindow(dpy, s), 10, 10, 660, 200, 1,
				BlackPixel(dpy, s), WhitePixel(dpy, s));
	XSelectInput(dpy, win, ExposureMask | KeyPressMask);
	XMapWindow(dpy, win);

#if defined(__APPLE_CC__)
	XStoreName(dpy, win, "Geeks3D.com - X11 window under Mac OS X");
#else
	XStoreName(dpy, win, "Geeks3D.com - X11 window under Linux");
#endif

	WM_DELETE_WINDOW = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &WM_DELETE_WINDOW, 1);

	uname_ok = 0;
	ret = uname(&sname);
	if (ret != -1) {
		uname_ok = 1;
	}

	while (1) {
		XNextEvent(dpy, &e);
		if (e.type == Expose) {
			y_offset = 20;

#if defined(__APPLE_CC__)
			s1 = "X11 test app under Mac OS X Lion";
#else
			s1 = "X11 test app under Linux";
#endif

			s2 = "(C)2012 Geeks3D.com";
			XDrawString(dpy, win, DefaultGC(dpy, s), 10, y_offset,
				    s1, strlen(s1));
			y_offset += 20;
			XDrawString(dpy, win, DefaultGC(dpy, s), 10, y_offset,
				    s2, strlen(s2));
			y_offset += 20;

			if (uname_ok) {
				buf[0] = '\0';

				sprintf(buf, "System information:");
				XDrawString(dpy, win, DefaultGC(dpy, s), 10,
					    y_offset, buf, strlen(buf));
				y_offset += 15;

				sprintf(buf, "- System: %s", sname.sysname);
				XDrawString(dpy, win, DefaultGC(dpy, s), 10,
					    y_offset, buf, strlen(buf));
				y_offset += 15;

				sprintf(buf, "- Release: %s", sname.release);
				XDrawString(dpy, win, DefaultGC(dpy, s), 10,
					    y_offset, buf, strlen(buf));
				y_offset += 15;

				sprintf(buf, "- Version: %s", sname.version);
				XDrawString(dpy, win, DefaultGC(dpy, s), 10,
					    y_offset, buf, strlen(buf));
				y_offset += 15;

				sprintf(buf, "- Machine: %s", sname.machine);
				XDrawString(dpy, win, DefaultGC(dpy, s), 10,
					    y_offset, buf, strlen(buf));
				y_offset += 20;
			}

			XWindowAttributes wa;
			XGetWindowAttributes(dpy, win, &wa);
			width = wa.width;
			height = wa.height;
			buf[0] = '\0';
			sprintf(buf, "Current window size: %dx%d", width,
				height);
			XDrawString(dpy, win, DefaultGC(dpy, s), 10, y_offset,
				    buf, strlen(buf));
			y_offset += 20;
		}

		if (e.type == KeyPress) {
			buf[0] = '\0';
			len =
			    XLookupString(&e.xkey, buf, sizeof buf, &keysym,
					  NULL);
			if (keysym == XK_Escape)
				break;
		}

		if ((e.type == ClientMessage) &&
		    ((unsigned int)(e.xclient.data.l[0]) ==
		     (unsigned int)WM_DELETE_WINDOW)) {
			break;
		}
	}

	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);
	return 0;
}
