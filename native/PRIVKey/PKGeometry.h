#ifndef PK_GEOMETRY_H
#define PK_GEOMETRY_H

/* Portable contour geometry. Coordinates and STL thickness are millimetres.
 * No lock profiles or manufacturer cutting data are part of this module.
 * An extrusion preserves the supplied silhouette only, not hidden geometry.
 */
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PK_MAX_POINTS 256

typedef struct { double x, y; } PKPoint;
typedef struct { double min_x, min_y, max_x, max_y; } PKBounds;
typedef struct { size_t a, b, c; } PKTriangle;

typedef enum {
    PK_OK = 0,
    PK_INVALID_ARGUMENT,
    PK_INVALID_COUNT,
    PK_INVALID_COORDINATE,
    PK_DUPLICATE_POINT,
    PK_SELF_INTERSECTION,
    PK_DEGENERATE_POLYGON,
    PK_INSUFFICIENT_CAPACITY,
    PK_INVALID_THICKNESS,
    PK_IO_ERROR,
    PK_TRIANGULATION_FAILED
} PKStatus;

/* Input is an open array: do not repeat the first point at the end.
 * Coordinates must be finite, inside [-1e9, 1e9], and the contour must be
 * simple. Consecutive straight-line vertices may be simplified for export.
 */
PKStatus pk_validate_polygon(const PKPoint *points, size_t count);
/* Signed area; counter-clockwise input is positive. Returns NAN for invalid
 * point/count arguments, but does not itself check polygon intersections. */
double pk_area(const PKPoint *points, size_t count);
PKStatus pk_bounds(const PKPoint *points, size_t count, PKBounds *bounds);
/* Returns CCW triangles referring to the original input indices. Collinear
 * intermediate vertices are omitted. Allocate count-2 slots. On a capacity
 * error, *written is the required number. */
PKStatus pk_triangulate(const PKPoint *points, size_t count,
                        PKTriangle *triangles, size_t capacity, size_t *written);
/* Writers do not close the stream. They flush and report output failures.
 * SVG retains input coordinates with an explicit millimetre width/height.
 * STL uses the same x/y coordinates, with bottom z=0 and top z=thickness. */
PKStatus pk_write_svg(FILE *stream, const PKPoint *points, size_t count);
PKStatus pk_write_stl(FILE *stream, const PKPoint *points, size_t count,
                     double thickness);
const char *pk_status_message(PKStatus status);

#ifdef __cplusplus
}
#endif
#endif
