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
#include <string.h>
#include <malloc.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xcb/xcb.h>
#include <xcb/xkb.h>
#include <xcb/xcb_util.h>
#include <xcb/xcb_ewmh.h>

uint32_t get_current_xkb_group(xcb_connection_t *connection) {
	xcb_xkb_get_state_cookie_t cookie = xcb_xkb_get_state(connection, XCB_XKB_ID_USE_CORE_KBD);
	xcb_generic_error_t *error;
	xcb_xkb_get_state_reply_t *reply = xcb_xkb_get_state_reply(connection, cookie, &error);
	uint32_t result;
	if (error != NULL) {
		fprintf(stderr, "xcb_xkb_get_state() failed with error code %d - returning 0\n", error->error_code);
		result = 0;
	} else {
		result = reply->group;
	}
	if (reply != NULL) {
		free(reply);
	}
	return result;
}

void set_current_xkb_group(xcb_connection_t *connection, uint32_t group) {
	xcb_void_cookie_t cookie = xcb_xkb_latch_lock_state(connection, XCB_XKB_ID_USE_CORE_KBD,
			0, 0, 1/*lockGroup=true*/, group/*groupLock*/, 0, 0, 0);
	xcb_generic_error_t *error = xcb_request_check(connection, cookie);
	if (error != NULL) {
		switch (error->error_code) {
			case XCB_VALUE:
				fprintf(stderr, "set_current_xkb_group(%d) failed with BadValue error\n", group);
				break;
			default:
				fprintf(stderr, "set_current_xkb_group(%d) failed with error code %d\n", group, error->error_code);
				break;
		}
		free(error);
	}
}

xcb_atom_t atom_NET_ACTIVE_WINDOW, atom_WTFKB_GROUP;

xcb_window_t get_active_window(xcb_connection_t *connection, xcb_window_t root) {
	xcb_get_property_cookie_t cookie = xcb_get_property(connection, 0, root, atom_NET_ACTIVE_WINDOW, XCB_ATOM_WINDOW, 0, 1);
	xcb_get_property_reply_t *reply = xcb_get_property_reply(connection, cookie, NULL);
	xcb_window_t result;
	if ((reply == NULL) || (xcb_get_property_value_length(reply) == 0)) {
		fprintf(stderr, "unable to get active window, returning root\n");
		result = root;
	} else {
		result = *(xcb_window_t*)xcb_get_property_value(reply);
	}
	if (reply != NULL) {
		free(reply);
	}
	return result;
}

uint32_t load_window_xkb_group(xcb_connection_t *connection, xcb_window_t window) {
	xcb_get_property_cookie_t cookie = xcb_get_property(connection, 0, window, atom_WTFKB_GROUP, XCB_GET_PROPERTY_TYPE_ANY, 0, 1);
	xcb_generic_error_t *error;
	xcb_get_property_reply_t *reply = xcb_get_property_reply(connection, cookie, &error);
	uint32_t result;
	if (error != NULL) {
		fprintf(stderr, "xcb_get_property(WTFKB_GROUP) failed with error code %d - returning 0\n", error->error_code);
		result = 0;
	} else if ((reply != NULL) && (xcb_get_property_value_length(reply) == 4)) {
		result = *(uint32_t*)xcb_get_property_value(reply);
	} else {
		printf("No WTFKB_GROUP property - returning 0\n");
		result = 0;
	}
	if (reply != NULL) {
		free(reply);
	}
	return result;
}

void save_window_xkb_group(xcb_connection_t *connection, xcb_window_t window, uint32_t group) {
	xcb_void_cookie_t cookie = xcb_change_property_checked(connection, XCB_PROP_MODE_REPLACE,
			window, atom_WTFKB_GROUP, XCB_ATOM_INTEGER, 32, 1, &group);
	xcb_generic_error_t *error = xcb_request_check(connection, cookie);
	if (error != NULL) {
		switch (error->error_code) {
			case XCB_WINDOW:
				printf("xcb_change_property(WTFKB_GROUP) failed because the window does not exist\n");
				break;
			default:
				fprintf(stderr, "xcb_change_property(WTFKB_GROUP) failed with error code %d\n", error->error_code);
				break;
		}
		free(error);
	}
}

xcb_screen_t *screen_nbr_to_screen(xcb_connection_t *connection, int screen_nbr) {
	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(connection));
	while (iter.rem > 0) {
		if ((screen_nbr--) == 0) {
			return iter.data;
		}
		xcb_screen_next(&iter);
	}
	return NULL;
}

xcb_atom_t get_atom_by_name(xcb_connection_t *connection, const char *name) {
	xcb_intern_atom_cookie_t iacookie = xcb_intern_atom(connection, 0, strlen(name), name);
	xcb_intern_atom_reply_t *iareply = xcb_intern_atom_reply(connection, iacookie, NULL);
	xcb_atom_t result = iareply->atom;
	free(iareply);
	return result;
}

int main(int argc, char **argv) {
	int screen_nbr;
	xcb_connection_t *connection = xcb_connect(NULL, &screen_nbr);
	xcb_screen_t *screen = screen_nbr_to_screen(connection, screen_nbr);
	if (screen == NULL) {
		fprintf(stderr, "No default screen found. Terminating\n");
		exit(1);
	}
	xcb_window_t root = screen->root;

	uint32_t evmask[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };
	xcb_void_cookie_t cookie = xcb_change_window_attributes(connection, root, XCB_CW_EVENT_MASK, evmask);
	xcb_generic_error_t *error = xcb_request_check(connection, cookie);
	if (error != NULL) {
		fprintf(stderr, "Changing root window attributes FAILED! Terminating.\n");
		exit(1);
	}
	free(error);

	int xkb_ok = xkb_x11_setup_xkb_extension(connection, XKB_X11_MIN_MAJOR_XKB_VERSION, XKB_X11_MIN_MINOR_XKB_VERSION, 0, NULL, NULL, NULL, NULL);
	if (!xkb_ok) {
		fprintf(stderr, "Failed to setup XKB extension.\n");
		exit(1);
	}

	atom_NET_ACTIVE_WINDOW = get_atom_by_name(connection, "_NET_ACTIVE_WINDOW");
	atom_WTFKB_GROUP = get_atom_by_name(connection, "WTFKB_GROUP");

	xcb_generic_event_t *event;
	xcb_window_t old_active_window = get_active_window(connection, root);
	while ((event = xcb_wait_for_event(connection))) {
		switch (event->response_type) { // & ~0x80) {
			case XCB_PROPERTY_NOTIFY:
				{
					xcb_property_notify_event_t *pnev = (xcb_property_notify_event_t*)event;
					if (pnev->atom == atom_NET_ACTIVE_WINDOW) {
						xcb_window_t new_active_window = get_active_window(connection, root);
						if (new_active_window != old_active_window) {
							int oldgroup = get_current_xkb_group(connection);
							int newgroup = load_window_xkb_group(connection, new_active_window);

							printf("window switch from %u(group %u) to %u(group %u)\n", old_active_window, oldgroup, new_active_window, newgroup);
							save_window_xkb_group(connection, old_active_window, oldgroup);
							set_current_xkb_group(connection, newgroup);

							old_active_window = new_active_window;
						}
					}
				}
				break;
			default:
				printf("unknown event\n");
				break;
		}
		free (event);
	}
	xcb_disconnect(connection);
	return 0;
}
