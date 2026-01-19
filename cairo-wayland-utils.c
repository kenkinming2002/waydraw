#include "cairo-wayland-utils.h"

#include "shm.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <unistd.h>

#include <sys/mman.h>

static void release_buffer(void *data, struct wl_buffer *wl_buffer)
{
  (void)data;
  wl_buffer_destroy(wl_buffer);
}

static struct wl_buffer_listener buffer_listener = {
  .release = &release_buffer,
};

struct wl_buffer *wl_buffer_from_cairo_surface(cairo_surface_t *surface,
                                               uint32_t *out_width,
                                               uint32_t *out_height,
                                               struct wl_shm *shm)
{
  cairo_surface_t *image = cairo_surface_map_to_image(surface, NULL);

  // Should we flush the original surface, the newly
  // mapped surface or neither?
  cairo_surface_flush(image);

  cairo_format_t format = cairo_image_surface_get_format(surface);
  assert(format == CAIRO_FORMAT_ARGB32);

  uint32_t width = cairo_image_surface_get_width(surface);
  uint32_t height = cairo_image_surface_get_height(surface);
  uint32_t stride = cairo_image_surface_get_stride(surface);
  assert(width * 4 == stride);

  void *data = cairo_image_surface_get_data(surface);

  uint32_t size = stride * height;

  int fd = allocate_shm_file(size);
  if (fd < 0) {
    fprintf(stderr, "error: failed to open shm file: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  void *storage = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (storage == MAP_FAILED) {
    fprintf(stderr, "error: failed to mmap shm file: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  memcpy(storage, data, size);
  cairo_surface_unmap_image(surface, image);

  struct wl_shm_pool *shm_pool = wl_shm_create_pool(shm, fd, size);
  struct wl_buffer *buffer = wl_shm_pool_create_buffer(
      shm_pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);

  wl_buffer_add_listener(buffer, &buffer_listener, NULL);

  wl_shm_pool_destroy(shm_pool);

  munmap(storage, size);
  close(fd);

  *out_width = width;
  *out_height = height;
  return buffer;
}

void wl_surface_update_from_cairo_surface(struct wl_surface *wl_surface,
                                          cairo_surface_t *surface,
                                          struct wl_shm *shm)
{
  uint32_t width, height;
  struct wl_buffer *buffer =
      wl_buffer_from_cairo_surface(surface, &width, &height, shm);

  wl_surface_attach(wl_surface, buffer, 0, 0);
  wl_surface_damage_buffer(wl_surface, 0, 0, width, height);
}

