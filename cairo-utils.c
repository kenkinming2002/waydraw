#include "cairo-utils.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

void cairo_image_surface_copy(cairo_surface_t *dst, cairo_surface_t *src)
{
  cairo_surface_flush(src);

  int src_width = cairo_image_surface_get_width(src);
  int src_height = cairo_image_surface_get_height(src);
  uint32_t *src_data = (uint32_t *)cairo_image_surface_get_data(src);

  int dst_width = cairo_image_surface_get_width(dst);
  int dst_height = cairo_image_surface_get_height(dst);
  uint32_t *dst_data = (uint32_t *)cairo_image_surface_get_data(dst);

  assert(src_width == dst_width);
  assert(src_height == dst_height);
  memcpy(dst_data, src_data, src_width * src_height * sizeof *src_data);

  cairo_surface_mark_dirty(dst);
}

cairo_surface_t *cairo_image_surface_clone(cairo_surface_t *surface)
{
  int width = cairo_image_surface_get_width(surface);
  int height = cairo_image_surface_get_height(surface);

  cairo_surface_t *new_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
  cairo_image_surface_copy(new_surface, surface);
  return new_surface;
}

