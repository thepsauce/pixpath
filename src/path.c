#include "pixpath.h"

static int edges_gen(XImage *image, struct edge **pEdges, Uint32 *pNumEdges)
{
	struct edge *edges = NULL, *newEdges = NULL;
	Uint32 numEdges = 0;

	unsigned long px;
	unsigned long surround[4];

	for (int y = 0; y < image->height; y++) {
		for (int x = 0; x < image->width; x++) {
			px = XGetPixel(image, x, y);
			if (px != glob_config.filled) {
				continue;
			}

			newEdges = realloc(edges, sizeof(*edges) *
					(numEdges + 4));
			if (newEdges == NULL) {
				free(edges);
				return -1;
			}
			edges = newEdges;

			for (enum edge_align a = 0; a < 4; a++) {
				surround[a] = glob_config.empty;
			}

			if (y + 1 < image->height) {
				surround[SOUTH] = XGetPixel(image, x, y + 1);
			}
			if (x > 0) {
				surround[WEST] = XGetPixel(image, x - 1, y);
			}
			if (y > 0) {
				surround[NORTH] = XGetPixel(image, x, y - 1);
			}
			if (x + 1 < image->width) {
				surround[EAST] = XGetPixel(image, x + 1, y);
			}

			if (surround[NORTH] == glob_config.empty &&
					surround[EAST] == glob_config.empty) {
				edges[numEdges].x = x;
				edges[numEdges].y = y;
				edges[numEdges].align = NORTH_EAST;
				numEdges++;
				surround[NORTH] = glob_config.filled;
				surround[EAST] = glob_config.filled;
			}
			if (surround[NORTH] == glob_config.empty &&
					surround[WEST] == glob_config.empty) {
				edges[numEdges].x = x;
				edges[numEdges].y = y;
				edges[numEdges].align = NORTH_WEST;
				numEdges++;
				surround[NORTH] = glob_config.filled;
				surround[WEST] = glob_config.filled;
			}
			if (surround[SOUTH] == glob_config.empty &&
					surround[EAST] == glob_config.empty) {
				edges[numEdges].x = x;
				edges[numEdges].y = y;
				edges[numEdges].align = SOUTH_EAST;
				numEdges++;
				surround[SOUTH] = glob_config.filled;
				surround[EAST] = glob_config.filled;
			}
			if (surround[SOUTH] == glob_config.empty &&
					surround[WEST] == glob_config.empty) {
				edges[numEdges].x = x;
				edges[numEdges].y = y;
				edges[numEdges].align = SOUTH_WEST;
				numEdges++;
				surround[SOUTH] = glob_config.filled;
				surround[WEST] = glob_config.filled;
			}

			for (enum edge_align a = 0; a < 4; a++) {
				if (surround[a] == glob_config.empty) {
					edges[numEdges].x = x;
					edges[numEdges].y = y;
					edges[numEdges].align = a;
					numEdges++;
				}
			}
		}
	}

	*pEdges = edges;
	*pNumEdges = numEdges;
	return 0;
}

static int groups_gen(struct edge *edges, Uint32 numEdges, int scale,
		struct group **pGroups, Uint32 *pNumGroups)
{
	struct group *groups;
	Uint32 numGroups;
	struct group group;

	if (scale < 5) {
		scale = 5;
	}

	groups = malloc(sizeof(*groups) * numEdges);
	numGroups = numEdges;
	for (Uint32 i = 0; i < numEdges; i++) {
		const struct edge e = edges[i];

		switch (e.align) {
		case NORTH:
			group.numPoints = 2;
			group.points[0].x = e.x;
			group.points[0].y = e.y;
			group.points[1].x = e.x + 1;
			group.points[1].y = e.y;
			break;
		case SOUTH:
			group.numPoints = 2;
			group.points[0].x = e.x + 1;
			group.points[0].y = e.y + 1;
			group.points[1].x = e.x;
			group.points[1].y = e.y + 1;
			break;
		case EAST:
			group.numPoints = 2;
			group.points[0].x = e.x + 1;
			group.points[0].y = e.y;
			group.points[1].x = e.x + 1;
			group.points[1].y = e.y + 1;
			break;
		case WEST:
			group.numPoints = 2;
			group.points[0].x = e.x;
			group.points[0].y = e.y + 1;
			group.points[1].x = e.x;
			group.points[1].y = e.y;
			break;
		case NORTH_EAST:
			group.numPoints = 3;
			group.points[0].x = e.x;
			group.points[0].y = e.y;
			group.points[1].x = e.x + 1;
			group.points[1].y = e.y;
			group.points[2].x = e.x + 1;
			group.points[2].y = e.y + 1;
			break;
		case NORTH_WEST:
			group.numPoints = 3;
			group.points[0].x = e.x;
			group.points[0].y = e.y + 1;
			group.points[1].x = e.x;
			group.points[1].y = e.y;
			group.points[2].x = e.x + 1;
			group.points[2].y = e.y;
			break;
		case SOUTH_EAST:
			group.numPoints = 3;
			group.points[0].x = e.x + 1;
			group.points[0].y = e.y;
			group.points[1].x = e.x + 1;
			group.points[1].y = e.y + 1;
			group.points[2].x = e.x;
			group.points[2].y = e.y + 1;
			break;
		case SOUTH_WEST:
			group.numPoints = 3;
			group.points[0].x = e.x + 1;
			group.points[0].y = e.y + 1;
			group.points[1].x = e.x;
			group.points[1].y = e.y + 1;
			group.points[2].x = e.x;
			group.points[2].y = e.y;
			break;
		}

		for (Uint32 j = 0; j < group.numPoints; j++) {
			group.points[j].x *= scale;
			group.points[j].y *= scale;
		}

		groups[i] = group;
	}

	*pGroups = groups;
	*pNumGroups = numGroups;
	return 0;
}

static void path_optimize(struct path *path)
{
	Uint32 i = 0;

	if (path->numPoints < 3) {
		return;
	}
	while (i < path->numPoints) {
		const Uint32 im = (i + 1) % path->numPoints;
		const Uint32 ir = (i + 2) % path->numPoints;
		const struct point l = path->points[i];
		const struct point m = path->points[im];
		const struct point r = path->points[ir];

		if ((l.x == m.x && m.x == r.x) || (l.y == m.y && m.y == r.y)) {
			path->numPoints--;
			/* remove middle point */
			memmove(&path->points[im], &path->points[ir],
					sizeof(*path->points) *
					(path->numPoints - im));
		} else {
			i++;
		}

	}
}

/* this function assumes that there are already points in the path */
static int path_join_group(struct path *path, const struct group *group)
{
	struct point *newPoints;

	newPoints = realloc(path->points, sizeof(*path->points) *
			(path->numPoints + group->numPoints -
			 /* exclude start point */ 1));
	if (newPoints == NULL) {
		return -1;
	}
	path->points = newPoints;
	memmove(&path->points[path->numPoints], &group->points[1],
			sizeof(*path->points) * (group->numPoints - 1));
	path->numPoints += group->numPoints - 1;
	return 0;
}

int paths_gen(XImage *image, int scale, struct path **pPaths, Uint32 *pNumPaths)
{
	struct edge *edges = NULL;
	Uint32 numEdges;

	struct group *groups = NULL;
	Uint32 numGroups;

	struct path *paths = NULL, *newPaths;
	Uint32 numPaths = 0;
	struct path *path = NULL;

	if (edges_gen(image, &edges, &numEdges) < 0) {
		goto error;
	}

	if (groups_gen(edges, numEdges, scale, &groups, &numGroups) < 0) {
		goto error;
	}

	free(edges);

	if (numGroups == 0) {
		return 1;
	}

	paths = malloc(sizeof(*paths));
	if (paths == NULL) {
		goto error;
	}
	path = &paths[0];
	numPaths = 1;
	path->numPoints = 0;

	while (numGroups > 0) {
		Bool found = False;

		if (path->numPoints == 0) {
			/* init path */
			const struct group g = groups[0];

			path->points = malloc(sizeof(*path->points) *
					g.numPoints);
			if (path->points == NULL) {
				goto error;
			}
			memcpy(&path->points[0], &g.points[0],
					sizeof(*path->points) * g.numPoints);
			path->numPoints = g.numPoints;

			/* remove first group */
			numGroups--;
			memmove(&groups[0], &groups[1], sizeof(*groups) *
					numGroups);
		}

		const struct point p = path->points[path->numPoints - 1];
		for (Uint32 i = 0; i < numGroups; i++) {
			const struct group g = groups[i];
			if (p.x == g.points[0].x && p.y == g.points[0].y) {
				if (path_join_group(path, &g) < 0) {
					goto error;
				}

				/* remove this group */
				numGroups--;
				memmove(&groups[i], &groups[i + 1],
						sizeof(*groups) *
						(numGroups - i));
				found = True;
				break;
			}
		}

		if (numGroups == 0) {
			break;
		}

		if (!found) {
			/* could not join this group, meaning it does not
		 	 * belong to this path: Make a new one
		 	 */
			newPaths = realloc(paths, sizeof(*paths) *
					(numPaths + 1));
			if (newPaths == NULL) {
				goto error;
			}
			paths = newPaths;
			path = &paths[numPaths++];
			path->numPoints = 0;
		}
	}

	free(groups);

	/* join continuous edges */
	for (Uint32 i = 0; i < numPaths; i++) {
		path_optimize(&paths[i]);
	}

	*pPaths = paths;
	*pNumPaths = numPaths;
	return 0;

error:
	free(edges);
	free(groups);
	paths_free(paths, numPaths);
	return -1;
}

int path_out_fontforge(struct path *path, FILE *fp)
{
	if (path->numPoints == 0) {
		return 1;
	}
	fprintf(fp, "pen.moveTo((%d,%d))\n",
			(int) (path->points[0].x * glob_config.scale.x +
				glob_config.translate.x),
			(int) (path->points[0].y * glob_config.scale.y +
				glob_config.translate.y));
	for (Uint32 i = 1; i < path->numPoints; i++) {
		const struct point p = path->points[i];
		fprintf(fp, "pen.lineTo((%d,%d))\n",
				(int) (p.x * glob_config.scale.x +
					glob_config.translate.x),
				(int) (p.y * glob_config.scale.y +
					glob_config.translate.y));
	}
	fprintf(fp, "pen.closePath()\n");
	return 0;
}

void paths_free(struct path *paths, Uint32 numPaths)
{
	if (paths == NULL) {
		return;
	}
	for (Uint32 i = 0; i < numPaths; i++) {
		free(paths[i].points);
	}
	free(paths);
}
