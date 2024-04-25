#include "pixpath.h"

struct config glob_config;

static Display *display;
static XftFont *font;
static int font_width, font_height;
static XftDraw *xftdraw;
static XftColor xftwhite, xftblack;
static Pixmap pixmap;
static GC gc;
static int unicode_point;

Uint32 codepoint_to_utf8(Uint32 codepoint, char *utf8)
{
	if (codepoint <= 0x7f) {
		utf8[0] = codepoint;
		utf8[1] = '\0';
		return 1;
	} else if (codepoint <= 0x7ff) {
		utf8[0] = 0xC0 | (char) (codepoint >> 6);
		utf8[1] = 0x80 | (char) (codepoint & 0x3f);
		utf8[2] = '\0';
		return 2;
	} else if (codepoint <= 0xffff) {
		utf8[0] = 0xE0 | (char) (codepoint >> 12);
		utf8[1] = 0x80 | (char) ((codepoint >> 6) & 0x3f);
		utf8[2] = 0x80 | (char) (codepoint & 0x3f);
		utf8[3] = '\0';
		return 3;
	} else if (codepoint <= 0x10ffff) {
		utf8[0] = 0xf0 | (char) (codepoint >> 18);
		utf8[1] = 0x80 | (char) ((codepoint >> 12) & 0x3f);
		utf8[2] = 0x80 | (char) ((codepoint >> 6) & 0x3f);
		utf8[3] = 0x80 | (char) (codepoint & 0x3f);
		utf8[4] = '\0';
		return 4;
	}
	return 0;
}

void next_unicode(int point)
{
	XImage *image;
	struct path *paths;
	Uint32 numPaths;

	unicode_point = point;
	if (XftCharExists(display, font, unicode_point)) {
		char utf8[8];

		codepoint_to_utf8(unicode_point, utf8);
		XSetForeground(display, gc, BlackPixel(display, 0));
		XFillRectangle(display, pixmap, gc, 0, 0,
				font_width, font_height);
		XftDrawStringUtf8(xftdraw, &xftwhite, font, 0, 12,
				(XftChar8*) utf8, strlen(utf8));

		image = XGetImage(display, pixmap, 0, 0,
				font_width, font_height, AllPlanes, ZPixmap);
		if (paths_gen(image, glob_config.prescale, &paths, &numPaths)
				== 0) {
			for (Uint32 i = 0; i < numPaths; i++) {
				path_out_fontforge(&paths[i], stdout);
			}
			paths_free(paths, numPaths);
			printf("\n");
		}
		XDestroyImage(image);
	}
}

int main(void)
{
	XRenderColor xrcolor;
	XEvent event;
	Window window;
	XGCValues values;
	int d = 0;
	char unichar[16];

	glob_config.filled = 0xffffff;
	glob_config.empty = 0x000000;
	/* hardcoded values for this specific font and fontforge */
	glob_config.prescale = 100;
	glob_config.scale = (struct fpoint) { 1.0f, -1.0f };
	glob_config.translate = (struct fpoint) { 0.0f, 1200.0f };

	display = XOpenDisplay(0);
	window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0,
			144, 144, 0, 0, 0);
	XSelectInput(display, window, ExposureMask | KeyPressMask);
	XMapWindow(display, window);

	font = XftFontOpenName(display, 0, "BmPlus IBM VGA 8x16");
	if (font == NULL) {
		return 1;
	}

	font_width = 4 * font->max_advance_width;
	font_height = font->height;

	pixmap = XCreatePixmap(display, window, font_width, font_height,
			DefaultDepth(display, 0));
	gc = XCreateGC(display, pixmap, 0, &values);
	XSetForeground(display, gc, 0x000000);

	xftdraw = XftDrawCreate(display, pixmap,
			DefaultVisual(display, 0), DefaultColormap(display, 0));
	xrcolor = (XRenderColor) {
		.red = 65535,
		.green = 65535,
		.blue = 65535,
		.alpha = 65535,
	};
	XftColorAllocValue(display, DefaultVisual(display, 0),
			DefaultColormap(display, 0), &xrcolor, &xftwhite);
	xrcolor = (XRenderColor) {
		.red = 0,
		.green = 0,
		.blue = 0,
		.alpha = 65535,
	};
	XftColorAllocValue(display, DefaultVisual(display, 0),
			DefaultColormap(display, 0), &xrcolor, &xftblack);

	next_unicode(0x2588);
	while (1) {
		while (XPending(display)) {
			XNextEvent(display, &event);
			if (event.type == KeyPress) {
				KeySym sym;

				sym = XLookupKeysym(&event.xkey, 0);
				if (sym == XK_l) {
					next_unicode(unicode_point + 1);
				} else if (sym == XK_h) {
					next_unicode(unicode_point - 1);
				}
				if ((sym >= XK_0 && sym <= XK_9) || (sym >= XK_a && sym <= XK_f)) {
					unichar[d++] = sym;
					printf("U+%.*s\n", d, unichar);
				}
				if (sym == XK_BackSpace) {
					if (d > 0) {
						d--;
						printf("U+%.*s\n", d, unichar);
					}
				}
				if (sym == XK_Return) {
					int cp = 0;

					for (int i = 0; i < d; i++) {
						cp *= 16;
						if (unichar[i] < 'a') {
							cp += unichar[i] - '0';
						} else {
							cp += unichar[i] - 'a' + 10;
						}
					}
					next_unicode(cp);
				}
			}
		}
		XCopyArea(display, pixmap, window, gc, 0, 0,
				font_width, font_height, 0, 0);
		usleep(10000);
	}

	XftColorFree(display, DefaultVisual(display, 0),
			DefaultColormap(display, 0), &xftwhite);
	XftColorFree(display, DefaultVisual(display, 0),
			DefaultColormap(display, 0), &xftblack);
	XftDrawDestroy(xftdraw);
	XftFontClose(display, font);
	XFreeGC(display, gc);
	XFreePixmap(display, pixmap);
	XDestroyWindow(display, window);
	XCloseDisplay(display);
	return 0;
}
