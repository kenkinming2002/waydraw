#ifndef CAIRO_WAYLAND_UTILS_H
#define CAIRO_WAYLAND_UTILS_H

#include <cairo.h>
#include <wayland-client.h>

/// Create a wayland buffer from a cairo surface. The created buffer is setup to
/// auto-release when the compositor is finished with using it.
struct wl_buffer *wl_buffer_from_cairo_surface(cairo_surface_t *surface,
                                               uint32_t *width,
                                               uint32_t *height,
                                               struct wl_shm *shm);

/// Update wayland surface with the content of a cairo surface.
void wl_surface_update_from_cairo_surface(struct wl_surface *wl_surface,
                                          cairo_surface_t *surface,
                                          struct wl_shm *shm);

#endif // CAIRO_WAYLAND_UTILS_H
