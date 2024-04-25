#ifndef INCLUDED_PIX_PATH_H
#define INCLUDED_PIX_PATH_H

#include <stdint.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>

typedef uint32_t Uint32;

struct fpoint {
	float x, y;
};

struct point {
	int x, y;
};

extern struct config {
	unsigned long filled;
	unsigned long empty;
	int prescale;
	struct fpoint scale;
	struct fpoint translate;
} glob_config;

Uint32 codepoint_to_utf8(Uint32 codepoint, char *utf8);

enum edge_align {
	NORTH, SOUTH, EAST, WEST,
	NORTH_EAST, SOUTH_EAST,
	NORTH_WEST, SOUTH_WEST,
};

struct edge {
	int x, y;
	enum edge_align align;
};

struct group {
	struct point points[3];
	/* 2 for an edge,
	 * 3 for a corner
	 */
	Uint32 numPoints;
};

struct path {
	struct point *points;
	Uint32 numPoints;
};

int path_out_fontforge(struct path *path, FILE *fp);
int paths_gen(XImage *image, int scale, struct path **pPaths, Uint32 *pNumPaths);
void paths_free(struct path *paths, Uint32 numPaths);

#endif
