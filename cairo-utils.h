#ifndef CAIRO_UTILS_H
#define CAIRO_UTILS_H

// Utilities functions for cairo surface.
//
// While it is not mentioned elsewhere, we assume that the only color format we
// ever work through out this code base is ARGB8888.

#include <cairo.h>

void cairo_image_surface_copy(cairo_surface_t *dst, cairo_surface_t *src);
cairo_surface_t *cairo_image_surface_clone(cairo_surface_t *surface);

#endif // CAIRO_UTILS_H
