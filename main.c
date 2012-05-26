/*

Copyright (c) 2012, Constantin S. Pan <kvapen@gmail.com>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

*/

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>

int get_current_xkb_group(Display *dpy) {
	XkbStateRec state;
	if (XkbGetState(dpy, XkbUseCoreKbd, &state) != Success) {
		printf("XkbGetState failed!\n");
		exit(1);
	}
	return state.group;
}

void set_current_xkb_group(Display *dpy, int group) {
	XkbLockGroup(dpy, XkbUseCoreKbd, group);
}

Window get_active_window(Display *dpy) {
	Atom some; int unused; unsigned long crap;
	unsigned long nitems;
	unsigned char *result;

	XGetWindowProperty(dpy, DefaultRootWindow(dpy), XInternAtom(dpy, "_NET_ACTIVE_WINDOW", 1),
			0, 1, False, AnyPropertyType, &some, &unused, &nitems, &crap, &result);
	Window window = *(Window*)result;
	XFree(result);
	return window;
}

int load_window_xkb_group(Display *dpy, Window w) {
	Atom some; int unused; unsigned long crap;
	unsigned long nitems;
	unsigned char *result;

	XGetWindowProperty(dpy, w, XInternAtom(dpy, "WTFKB_GROUP", 0),
			0, 32, False, XInternAtom(dpy, "WTFKB_GROUP", 0),
			&some, &unused, &nitems, &crap, &result);
	if (nitems == 1) {
		int group = *(int*)result;
		XFree(result);
		return group;
	} else {
		printf("WTFKB_GROUP is unset for the window, "
				"or the window does not exist any more. "
				"Returning current\n");
		return get_current_xkb_group(dpy);
	}
}

void save_window_xkb_group(Display *dpy, Window w, int group) {
	XChangeProperty(dpy, w, XInternAtom(dpy, "WTFKB_GROUP", 0),
			XInternAtom(dpy, "WTFKB_GROUP", 0), 32,
			PropModeReplace, (unsigned char *)&group, 1);
}

int error_handler(Display *dpy, XErrorEvent *e) {
	printf("An XError occurred: perhaps some window or atom does not exist, ignoring.\n");
	return 0;
}

int main(int argc, char **argv) {
	Display *dpy = XOpenDisplay(0);
	Window root = DefaultRootWindow(dpy);
	XEvent ev;

	XSelectInput(dpy, root, PropertyChangeMask);
	XSetErrorHandler(&error_handler);

	Window old_active_window = get_active_window(dpy);
	while (1) {
		XNextEvent(dpy, &ev);
		XPropertyEvent *pe = NULL;
		switch (ev.type) {
			case PropertyNotify:
				pe = (XPropertyEvent*)&ev;
				if (pe->atom == XInternAtom(dpy, "_NET_ACTIVE_WINDOW", 1)) {
					Window new_active_window = get_active_window(dpy);
					if (new_active_window != old_active_window) {
						int oldgroup = get_current_xkb_group(dpy);
						int newgroup = load_window_xkb_group(dpy, new_active_window);

						printf("window switch from %lu(group %d) to %lu(group %d)\n", old_active_window, oldgroup, new_active_window, newgroup);
						save_window_xkb_group(dpy, old_active_window, oldgroup);
						set_current_xkb_group(dpy, newgroup);

						old_active_window = new_active_window;
					}
				}
				break;
			default:
				printf("other\n");
				break;
		}
	}
	XCloseDisplay(dpy);
	return 0;
}
