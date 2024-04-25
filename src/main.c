#include "pixpath.h"

struct config glob_config;

Uint32 codepoint_to_utf8(FT_UInt codepoint, char *utf8)
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
	utf8[0] = '\0';
	return 0;
}

struct render_context {
	Display *display;
	int screen;
	XftFont *font;
	XftColor white, black;
	Pixmap pixmap;
	GC gc;
	XftDraw *draw;
	int width;
	int height;
} render_context;

int render_context_init(const char *xftFont)
{
	XRenderColor xrcolor;

	if (glob_config.loglevel == LOG_VERBOSE) {
		fprintf(stderr, "info: initializing render context (X)\n");
	}
	render_context.display = XOpenDisplay(0);
	if (render_context.display == NULL) {
		fprintf(stderr, "error: could not open connection to X server\n");
		return -1;
	}
	render_context.screen = DefaultScreen(render_context.display);

	render_context.font = XftFontOpenName(render_context.display,
			DefaultScreen(render_context.display),
			xftFont);
	if (render_context.font == NULL) {
		fprintf(stderr, "error: could not open font '%s'\n", xftFont);
		XCloseDisplay(render_context.display);
		return -1;
	}

	xrcolor = (XRenderColor) {
		.red = 65535,
		.green = 65535,
		.blue = 65535,
		.alpha = 65535,
	};
	if (!XftColorAllocValue(render_context.display,
			DefaultVisual(render_context.display,
				render_context.screen),
			DefaultColormap(render_context.display,
				render_context.screen),
			&xrcolor,
			&render_context.white)) {
		fprintf(stderr, "error: could not allocate xft white\n");
		XftFontClose(render_context.display, render_context.font);
		XCloseDisplay(render_context.display);
		return -1;
	}

	xrcolor = (XRenderColor) {
		.red = 0,
		.green = 0,
		.blue = 0,
		.alpha = 65535,
	};
	if (!XftColorAllocValue(render_context.display,
			DefaultVisual(render_context.display,
				render_context.screen),
			DefaultColormap(render_context.display,
				render_context.screen),
			&xrcolor,
			&render_context.black)) {
		fprintf(stderr, "error: could not allocate xft black\n");
		XftFontClose(render_context.display, render_context.font);
		XCloseDisplay(render_context.display);
		return -1;
	}
	if (glob_config.loglevel == LOG_VERBOSE) {
		fprintf(stderr, "sucess: initialized render context (X)\n");
	}
	return 0;
}

void render_context_uninit(void)
{
	if (glob_config.loglevel == LOG_VERBOSE) {
		fprintf(stderr, "info: uninitializing render context (X)\n");
	}
	if (render_context.pixmap != None) {
		XftDrawDestroy(render_context.draw);
		XFreeGC(render_context.display, render_context.gc);
		XFreePixmap(render_context.display, render_context.pixmap);
	}

	XftColorFree(render_context.display,
			DefaultVisual(render_context.display,
				render_context.screen),
			DefaultColormap(render_context.display,
				render_context.screen),
			&render_context.white);
	XftColorFree(render_context.display,
			DefaultVisual(render_context.display,
				render_context.screen),
			DefaultColormap(render_context.display,
				render_context.screen),
			&render_context.black);
	XftFontClose(render_context.display, render_context.font);
	XCloseDisplay(render_context.display);
}

int render_context_buffer(int width, int height)
{
	Pixmap pixmap;
	GC gc;
	XGCValues values;
	XftDraw *draw;

	if (glob_config.loglevel == LOG_VERBOSE) {
		fprintf(stderr, "info: increasing buffer size to %dx%d\n", width, height);
	}

	if (width <= render_context.width && height <= render_context.height) {
		if (glob_config.loglevel == LOG_VERBOSE) {
			fprintf(stderr, "info: buffer is already large enough at %dx%d\n", width, height);
		}
		return 1;
	}

	if (width < render_context.width) {
		width = render_context.width;
	}

	if (height < render_context.height) {
		height = render_context.height;
	}

	pixmap = XCreatePixmap(render_context.display,
			DefaultRootWindow(render_context.display),
			width, height,
			DefaultDepth(render_context.display,
				render_context.screen));
	if (pixmap == None) {
		fprintf(stderr, "error: could not create pixmap\n");
		return -1;
	}

	gc = XCreateGC(render_context.display, pixmap, 0, &values);
	if (gc == None) {
		fprintf(stderr, "error: could not create gc\n");
		XFreePixmap(render_context.display, pixmap);
		return -1;
	}
	XSetForeground(render_context.display, gc, 0x000000);

	draw = XftDrawCreate(render_context.display, pixmap,
			DefaultVisual(render_context.display,
				render_context.screen),
			DefaultColormap(render_context.display,
				render_context.screen));
	if (draw == NULL) {
		fprintf(stderr, "error: could not create xft draw\n");
		XFreePixmap(render_context.display, pixmap);
		XFreeGC(render_context.display, gc);
		return -1;
	}

	if (render_context.pixmap != None) {
		XftDrawDestroy(render_context.draw);
		XFreeGC(render_context.display, render_context.gc);
		XFreePixmap(render_context.display, render_context.pixmap);
	}

	render_context.pixmap = pixmap;
	render_context.gc = gc;
	render_context.draw = draw;
	render_context.width = width;
	render_context.height = height;

	if (glob_config.loglevel == LOG_VERBOSE) {
		fprintf(stderr, "success: increased buffer\n");
	}
	return 0;
}

int next_unicode(FT_UInt point)
{
	XGlyphInfo extents;
	XImage *image;
	struct path *paths;
	Uint32 numPaths;
	char utf8[8];

	if (glob_config.loglevel == LOG_VERBOSE) {
		fprintf(stderr, "info: doing unicode codepoint U+%u\n", point);
	}

	if (!XftCharExists(render_context.display, render_context.font, point)) {
		if (glob_config.loglevel == LOG_VERBOSE) {
			fprintf(stderr, "fail: code point does not exist\n");
		}
		return 1;
	}

	XftGlyphExtents(render_context.display, render_context.font, &point, 1,
			&extents);

	if (extents.width == 0) {
		extents.width = render_context.font->height;
		extents.height = render_context.font->height;
		if (glob_config.loglevel == LOG_VERBOSE) {
			fprintf(stderr, "info: glyph has no extent\n");
		}
	} else {
		if (glob_config.loglevel == LOG_VERBOSE) {
			fprintf(stderr, "info: glyph extent is %dx%d\n",
					extents.width, extents.height);
		}
	}

	if (render_context_buffer(extents.width, extents.height) < 0) {
		/* prints their own error message */
		return -1;
	}

	codepoint_to_utf8(point, utf8);
	XFillRectangle(render_context.display, render_context.pixmap,
			render_context.gc, 0, 0, extents.width, extents.height);
	XftDrawStringUtf8(render_context.draw, &render_context.white,
			render_context.font, 0, 12, /* TODO: hardcoded 12 */
			(XftChar8*) utf8, strlen(utf8));

	image = XGetImage(render_context.display, render_context.pixmap,
			0, 0, extents.width, extents.height,
			AllPlanes, ZPixmap);

	const int r = paths_gen(image, glob_config.prescale, &paths, &numPaths);
	if (r > 0) {
		if (glob_config.loglevel == LOG_VERBOSE) {
			fprintf(stderr, "fail: glyph is empty\n");
		}
	} else if (r == 0) {
		int maxExtent = 0;
		/* TODO: hardcode 8 value */
		const int units = 8 * glob_config.prescale;

		printf("glyph = font.createChar(ord(\"\\U%08x\"))\n", point);
		printf("pen = glyph.glyphPen()\n");
		for (Uint32 i = 0; i < numPaths; i++) {
			const struct path p = paths[i];
			path_out_fontforge(&p, stdout);
			for (Uint32 j = 0; j < p.numPoints; j++) {
				if (p.points[j].x > maxExtent) {
					maxExtent = p.points[j].x;
				}
			}
		}
		maxExtent = (maxExtent + units - 1) / units * units;
		printf("glyph.width = %d\n", maxExtent);
		printf("\n");
		paths_free(paths, numPaths);

		if (glob_config.loglevel == LOG_VERBOSE) {
			fprintf(stderr, "success: path was written\n");
		}
	} else {
		fprintf(stderr, "error: could not handle glyph\n");
	}

	XDestroyImage(image);
	return r;
}

int main(int argc, char **argv)
{
	glob_config.loglevel = LOG_DEFAULT;
	glob_config.filled = 0xffffff;
	glob_config.empty = 0x000000;
	/* TODO: hardcoded values for this specific font and fontforge */
	glob_config.prescale = 100;
	glob_config.scale = (struct fpoint) { 1.0f, -1.0f };
	glob_config.translate = (struct fpoint) { 0.0f, 1200.0f };

	if (render_context_init("BmPlus IBM VGA 8x16") < 0) {
		/* prints their own error message */
		return -1;
	}

	(void) argc; (void) argv;
	/* TODO: hardcoded ascent/descent */
	printf("import fontforge\nfont = fontforge.font()\nfont.ascent = 1200\nfont.descent = 400\nfont.em = 1600\nfont.upos = -350\n");
	for (FT_UInt i = 0; i <= 0x10ffff; i++) {
		next_unicode(i);
	}
	next_unicode('!');
	printf("\nfont.generate(\"out.ttf\")\nfont.close()\n");

	render_context_uninit();

	return 0;
}
