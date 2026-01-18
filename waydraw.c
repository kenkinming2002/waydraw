#include "snapshot.h"
#include "shm.h"

#include "cairo-utils.h"

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include <xkbcommon/xkbcommon.h>

#include <wayland-util.h>
#include <xdg-shell-client-protocol.h>
#include <wlr-layer-shell-unstable-v1-client-protocol.h>

#include <assert.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <unistd.h>

#include <sys/mman.h>

#include <linux/input-event-codes.h>

#define SCROLL_SENSITIVITY 0.1
#define MIN_DRAW_RADIUS 1

static double COLOR_PALLETE[][4] = {
  { 1.0, 0.0, 0.0, 1.0, },
  { 0.0, 1.0, 0.0, 1.0, },
  { 0.0, 0.0, 1.0, 1.0, },
  { 1.0, 1.0, 0.0, 1.0, },
  { 1.0, 0.0, 1.0, 1.0, },
  { 0.0, 1.0, 1.0, 1.0, },
};

#define COLOR_PALLETE_SIZE (sizeof COLOR_PALLETE / sizeof COLOR_PALLETE[0])

enum waydraw_mode
{
  WAYDRAW_MODE_BRUSH,
  WAYDRAW_MODE_RECTANGLE,
  WAYDRAW_MODE_CIRCLE,

  WAYDRAW_MODE_COUNT,
};

struct waydraw_output
{
  struct waydraw *waydraw;

  struct wl_list link;

  struct wl_output *wl_output;
  struct wl_surface *wl_surface;
  struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1;

  struct snapshot *snapshot;
};

struct waydraw_seat
{
  struct wl_seat *wl_seat;

  struct wl_keyboard *wl_keyboard;

  struct xkb_context *xkb_context;
  struct xkb_state *xkb_state;

  struct wl_pointer *wl_pointer;
  struct wl_surface *wl_pointer_surface;

  struct waydraw_output *keyboard_focus;
  struct waydraw_output *pointer_focus;

  double x, y;

  unsigned color_index;
  double weight;

  enum waydraw_mode mode;
  enum waydraw_mode committed_mode;

  double shape_x, shape_y;

  struct waydraw_output *drawing_focus;

  cairo_surface_t *surface;
  cairo_surface_t *base_surface;
  cairo_t *cairo;
};

struct waydraw
{
  struct wl_display *wl_display;

  struct wl_compositor *wl_compositor;
  struct wl_shm *wl_shm;
  struct zwlr_layer_shell_v1 *zwlr_layer_shell_v1;

  bool initialized;

  struct waydraw_seat seat;
  struct wl_list outputs;
};

static void handle_global(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version);

static void handle_output(struct waydraw *waydraw, struct wl_output *wl_output);
static void handle_seat(struct waydraw *waydraw, struct wl_seat *wl_seat);

static void init_output(struct waydraw_output *output);
static void update_output(struct waydraw_output *output);

static void update_cursor(struct waydraw_seat *seat);

static void seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities);

static void keyboard_enter(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys);
static void keyboard_leave(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface);

static void handle_keymap(void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size);
static void handle_key(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
static void handle_modifiers(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group);

static void pointer_enter(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y);
static void pointer_leave(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface);

static void pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y);
static void pointer_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
static void pointer_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value);

static void configure_surface(void *data, struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1, uint32_t serial, uint32_t width, uint32_t height);
static void release_buffer(void *data, struct wl_buffer *wl_buffer);

static struct wl_buffer *create_buffer(const struct waydraw *waydraw, uint32_t width, uint32_t height, void *data);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored  "-Wincompatible-pointer-types"

static void noop() {}

static struct wl_registry_listener wl_registry_listener = {
  .global = &handle_global,
  .global_remove = &noop,
};

static struct wl_seat_listener wl_seat_listener = {
  .capabilities = &seat_capabilities,
  .name = &noop,
};

static struct wl_keyboard_listener wl_keyboard_listener = {
  .keymap = &handle_keymap,
  .enter = &keyboard_enter,
  .leave = &keyboard_leave,
  .key = &handle_key,
  .modifiers = &handle_modifiers,
  .repeat_info = &noop,
};

static struct wl_pointer_listener wl_pointer_listener = {
  .enter = &pointer_enter,
  .leave = &pointer_leave,
  .motion = &pointer_motion,
  .button = &pointer_button,
  .axis = &pointer_axis,
  .frame = &noop,
  .axis_source = &noop,
  .axis_stop = &noop,
  .axis_discrete = &noop,
  .axis_value120 = &noop,
  .axis_relative_direction = &noop,
};

static struct zwlr_layer_surface_v1_listener zwlr_layer_surface_v1_listener = {
  .configure = &configure_surface,
  .closed = &noop,
};

static struct wl_buffer_listener wl_buffer_listener = {
  .release = &release_buffer,
};

#pragma GCC diagnostic pop

static void handle_global(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version)
{
  struct waydraw *waydraw = data;

  if(strcmp(interface, wl_compositor_interface.name) == 0)
  {
    waydraw->wl_compositor = wl_registry_bind(wl_registry, name, &wl_compositor_interface, version);
    return;
  }

  if(strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0)
  {
    waydraw->zwlr_layer_shell_v1 = wl_registry_bind(wl_registry, name, &zwlr_layer_shell_v1_interface, version);
    return;
  }

  if(strcmp(interface, wl_shm_interface.name) == 0)
  {
    waydraw->wl_shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, version);
    return;
  }

  if(strcmp(interface, wl_output_interface.name) == 0)
  {
    handle_output(waydraw, wl_registry_bind(wl_registry, name, &wl_output_interface, version));
    return;
  }

  if(strcmp(interface, wl_seat_interface.name) == 0)
  {
    handle_seat(waydraw, wl_registry_bind(wl_registry, name, &wl_seat_interface, version));
    return;
  }
}

static void handle_output(struct waydraw *waydraw, struct wl_output *wl_output)
{
  struct waydraw_output *output = calloc(1, sizeof *output);
  output->waydraw = waydraw;
  output->wl_output = wl_output;
  wl_list_insert(&waydraw->outputs, &output->link);

  if(waydraw->initialized)
    init_output(output);
}

static void handle_seat(struct waydraw *waydraw, struct wl_seat *wl_seat)
{
  waydraw->seat.wl_seat = wl_seat;
  wl_seat_add_listener(waydraw->seat.wl_seat, &wl_seat_listener, &waydraw->seat);
}

static void init_output(struct waydraw_output *output)
{
  struct waydraw *waydraw = output->waydraw;

  output->wl_surface = wl_compositor_create_surface(waydraw->wl_compositor);
  wl_surface_set_user_data(output->wl_surface, output);

  output->zwlr_layer_surface_v1 = zwlr_layer_shell_v1_get_layer_surface(
      waydraw->zwlr_layer_shell_v1,
      output->wl_surface,
      output->wl_output,
      ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
      "waydraw");

  zwlr_layer_surface_v1_add_listener(output->zwlr_layer_surface_v1, &zwlr_layer_surface_v1_listener, output);

  zwlr_layer_surface_v1_set_keyboard_interactivity(output->zwlr_layer_surface_v1, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
  zwlr_layer_surface_v1_set_exclusive_zone(output->zwlr_layer_surface_v1, -1);
  zwlr_layer_surface_v1_set_anchor(output->zwlr_layer_surface_v1,
      ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
      ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
      ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
      ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);

  wl_surface_commit(output->wl_surface);
}

static void update_output(struct waydraw_output *output)
{
  uint32_t width, height;
  uint32_t *data;
  snapshot_map(output->snapshot, &width, &height, &data);

  struct wl_buffer *buffer = create_buffer(output->waydraw, width, height, data);
  wl_surface_attach(output->wl_surface, buffer, 0, 0);
  wl_surface_damage_buffer(output->wl_surface, 0, 0, width, height);
  wl_surface_commit(output->wl_surface);
}

static void update_cursor(struct waydraw_seat *seat)
{
  struct waydraw *waydraw = wl_container_of(seat, waydraw, seat);

  int size = ceil(seat->weight);
  int hsize = round(size * 0.5);

  cairo_surface_t *cairo_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
  cairo_t *cairo = cairo_create(cairo_surface);
  cairo_set_source_rgba(cairo,
      COLOR_PALLETE[seat->color_index][0],
      COLOR_PALLETE[seat->color_index][1],
      COLOR_PALLETE[seat->color_index][2],
      COLOR_PALLETE[seat->color_index][3]
  );
  cairo_set_line_width(cairo, 3);
  cairo_arc(cairo, hsize, hsize, seat->weight * 0.5, 0, 2*M_PI);
  cairo_fill(cairo);
  cairo_surface_flush(cairo_surface);

  uint32_t *data = (uint32_t *)cairo_image_surface_get_data(cairo_surface);

  struct wl_buffer *buffer = create_buffer(waydraw, size, size, data);
  wl_surface_attach(seat->wl_pointer_surface, buffer, 0, 0);
  wl_surface_damage_buffer(seat->wl_pointer_surface, 0, 0, size, size);
  wl_surface_commit(seat->wl_pointer_surface);

  cairo_destroy(cairo);
  cairo_surface_destroy(cairo_surface);
}

static void seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities)
{
  struct waydraw_seat *seat = data;
  struct waydraw *waydraw = wl_container_of(seat, waydraw, seat);

  if(seat->wl_keyboard)
    wl_keyboard_destroy(seat->wl_keyboard);

  if(seat->wl_pointer)
    wl_pointer_destroy(seat->wl_pointer);

  if(seat->wl_pointer_surface)
    wl_surface_destroy(seat->wl_pointer_surface);

  if(capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
  {
    seat->wl_keyboard = wl_seat_get_keyboard(wl_seat);
    wl_keyboard_add_listener(seat->wl_keyboard, &wl_keyboard_listener, seat);
  }
  else
    seat->wl_keyboard = NULL;

  if(capabilities & WL_SEAT_CAPABILITY_POINTER)
  {
    seat->wl_pointer = wl_seat_get_pointer(wl_seat);
    wl_pointer_add_listener(seat->wl_pointer, &wl_pointer_listener, seat);
    seat->wl_pointer_surface = wl_compositor_create_surface(waydraw->wl_compositor);
  }
  else
  {
    seat->wl_pointer = NULL;
    seat->wl_pointer_surface = NULL;
  }
}

static void keyboard_enter(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
  (void)wl_keyboard;
  (void)serial;
  (void)keys;

  struct waydraw_seat *seat = data;
  assert(seat->keyboard_focus == NULL);
  seat->keyboard_focus = wl_surface_get_user_data(surface);
}

static void keyboard_leave(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface)
{
  (void)wl_keyboard;
  (void)serial;

  struct waydraw_seat *seat = data;
  assert(seat->keyboard_focus == wl_surface_get_user_data(surface));
  seat->keyboard_focus = NULL;
}

static void handle_keymap(void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size)
{
  (void)wl_keyboard;

  struct waydraw_seat *seat = data;

  if(seat->xkb_state)
    xkb_state_unref(seat->xkb_state);

  assert(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);

  char *map_shm = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if(map_shm == MAP_FAILED)
  {
    fprintf(stderr, "error: failed to mmap keymap file: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  struct xkb_keymap *xkb_keymap = xkb_keymap_new_from_string(
      seat->xkb_context,
      map_shm,
      XKB_KEYMAP_FORMAT_TEXT_V1,
      XKB_KEYMAP_COMPILE_NO_FLAGS);

  if(!xkb_keymap)
  {
    fprintf(stderr, "error: failed to parse keymap file\n");
    exit(EXIT_FAILURE);
  }

  seat->xkb_state = xkb_state_new(xkb_keymap);
  if(!seat->xkb_state)
  {
    fprintf(stderr, "error: failed to create xkb state from keymap\n");
    exit(EXIT_FAILURE);
  }

  xkb_keymap_unref(xkb_keymap);
  munmap(map_shm, size);
  close(fd);
}

static void handle_key(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
  (void)wl_keyboard;
  (void)serial;
  (void)time;

  struct waydraw_seat *seat = data;
  struct waydraw_output *output = seat->keyboard_focus;
  assert(seat->xkb_state);
  assert(output);

  if(state == WL_KEYBOARD_KEY_STATE_PRESSED)
  {
    xkb_keysym_t sym = xkb_state_key_get_one_sym(seat->xkb_state, key + 8);
    switch(sym)
    {
    case XKB_KEY_z:
      if(xkb_state_mod_name_is_active(seat->xkb_state, "Control", XKB_STATE_MODS_EFFECTIVE))
      {
        snapshot_undo(output->snapshot);
        update_output(output);
      }
      break;
    case XKB_KEY_Z:
      if(xkb_state_mod_name_is_active(seat->xkb_state, "Control", XKB_STATE_MODS_EFFECTIVE))
      {
        snapshot_redo(output->snapshot);
        update_output(output);
      }
      break;
    case XKB_KEY_x:
      if(xkb_state_mod_name_is_active(seat->xkb_state, "Control", XKB_STATE_MODS_EFFECTIVE))
      {
        snapshot_earlier(output->snapshot);
        update_output(output);
      }
      break;
    case XKB_KEY_X:
      if(xkb_state_mod_name_is_active(seat->xkb_state, "Control", XKB_STATE_MODS_EFFECTIVE))
      {
        snapshot_later(output->snapshot);
        update_output(output);
      }
      break;
    case XKB_KEY_T: // This is shift-tab. Don't ask me why.
      if(seat->mode == 0)
        seat->mode = WAYDRAW_MODE_COUNT - 1;
      else
        seat->mode -= 1;
      update_cursor(seat);
      break;
    case XKB_KEY_t:
      if(seat->mode == WAYDRAW_MODE_COUNT - 1)
        seat->mode = 0;
      else
        seat->mode += 1;
      update_cursor(seat);
      break;
    case XKB_KEY_ISO_Left_Tab: // This is shift-tab. Don't ask me why.
      if(seat->color_index == 0)
        seat->color_index = COLOR_PALLETE_SIZE - 1;
      else
        seat->color_index -= 1;
      update_cursor(seat);
      break;
    case XKB_KEY_Tab:
      if(seat->color_index == COLOR_PALLETE_SIZE - 1)
        seat->color_index = 0;
      else
        seat->color_index += 1;
      update_cursor(seat);
      break;
    case XKB_KEY_q:
      exit(EXIT_SUCCESS);
      break;
    }
  }
}

static void handle_modifiers(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
  (void)wl_keyboard;
  (void)serial;

  struct waydraw_seat *seat = data;
  assert(seat->xkb_state);
  xkb_state_update_mask(seat->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static void pointer_enter(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
  struct waydraw_seat *seat = data;
  assert(seat->pointer_focus == NULL);
  seat->pointer_focus = wl_surface_get_user_data(surface);

  seat->x = wl_fixed_to_double(surface_x);
  seat->y = wl_fixed_to_double(surface_y);

  int size = ceil(seat->weight);
  int hsize = round(size * 0.5);

  update_cursor(seat);
  wl_pointer_set_cursor(wl_pointer, serial, seat->wl_pointer_surface, hsize, hsize);
}

static void pointer_leave(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface)
{
  (void)wl_pointer;
  (void)serial;

  struct waydraw_seat *seat = data;
  assert(seat->pointer_focus == wl_surface_get_user_data(surface));
  seat->pointer_focus = NULL;
}

static void pointer_motion(void *data, struct wl_pointer *wl_pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y)
{
  (void)wl_pointer;
  (void)time;

  struct waydraw_seat *seat = data;
  struct waydraw_output *output = seat->pointer_focus;
  assert(output);

  seat->x = wl_fixed_to_double(surface_x);
  seat->y = wl_fixed_to_double(surface_y);

  // Note: There is a bit of an out-of-sync problem that could happen but should
  //       not matter. The cairo surface we draw on is from the current node we
  //       push onto the snapshot tree. It could happen that the user undo when
  //       we are drawing and the current node will change, in which case we
  //       will be drawing to a surface that is no longer rendered. The call to
  //       update_output only redraw the output by copying from the surface on
  //       the current node on the snapshot tree, which we are not actually
  //       drawing onto. We could technically try to work around that but there
  //       is no need to.
  if(seat->drawing_focus)
  {
    switch(seat->committed_mode)
    {
    case WAYDRAW_MODE_BRUSH:
      cairo_line_to(seat->cairo, seat->x, seat->y);
      cairo_stroke_preserve(seat->cairo);
      break;
    case WAYDRAW_MODE_RECTANGLE:
      {
        double width = seat->x - seat->shape_x;
        double height = seat->y - seat->shape_y;

        cairo_image_surface_copy(seat->surface, seat->base_surface);
        cairo_rectangle(seat->cairo, seat->shape_x, seat->shape_y, width, height);
        cairo_stroke(seat->cairo);
      }
      break;
    case WAYDRAW_MODE_CIRCLE:
      {
        double dx = seat->x - seat->shape_x;
        double dy = seat->y - seat->shape_y;
        double radius = sqrt(dx * dx + dy * dy);

        cairo_image_surface_copy(seat->surface, seat->base_surface);
        cairo_arc(seat->cairo, seat->shape_x, seat->shape_y, radius, 0, 2.0 * M_PI);
        cairo_stroke(seat->cairo);
      }
      break;
    case WAYDRAW_MODE_COUNT:
      break;
    }

    update_output(seat->drawing_focus);
  }
}

static void pointer_button(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
  (void)wl_pointer;
  (void)serial;
  (void)time;

  struct waydraw_seat *seat = data;
  if(button == BTN_LEFT)
    switch(state)
    {
    case WL_POINTER_BUTTON_STATE_PRESSED:
      // Do not allow drawing across outputs in a single stroke.
      if(!seat->drawing_focus)
      {
        seat->committed_mode = seat->mode;
        seat->drawing_focus = seat->pointer_focus;

        snapshot_push(seat->drawing_focus->snapshot);

        seat->surface = cairo_surface_reference(seat->drawing_focus->snapshot->current->cairo_surface);
        if(seat->committed_mode == WAYDRAW_MODE_BRUSH)
          seat->base_surface = NULL;
        else
          seat->base_surface = cairo_image_surface_clone(seat->surface);

        seat->cairo = cairo_create(seat->surface);

        cairo_set_source_rgba(seat->cairo,
            COLOR_PALLETE[seat->color_index][0],
            COLOR_PALLETE[seat->color_index][1],
            COLOR_PALLETE[seat->color_index][2],
            COLOR_PALLETE[seat->color_index][3]
        );
        cairo_set_line_width(seat->cairo, seat->weight);
        cairo_set_line_cap(seat->cairo, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(seat->cairo, CAIRO_LINE_JOIN_ROUND);
        cairo_move_to(seat->cairo, seat->x, seat->y);

        if(seat->committed_mode == WAYDRAW_MODE_BRUSH)
        {
          cairo_new_path(seat->cairo);
        }
        else
        {
          seat->shape_x = seat->x;
          seat->shape_y = seat->y;
        }
      }
      break;
    case WL_POINTER_BUTTON_STATE_RELEASED:
      if(seat->drawing_focus)
      {
        cairo_surface_destroy(seat->surface);
        if(seat->base_surface)
          cairo_surface_destroy(seat->base_surface);

        cairo_destroy(seat->cairo);

        seat->cairo = NULL;
        seat->drawing_focus = NULL;
      }
      break;
    }
}

static void pointer_axis(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
  (void)wl_pointer;
  (void)time;

  struct waydraw_seat *seat = data;
  if(axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
  {
    int old_size = ceil(seat->weight);
    int old_hsize = round(old_size * 0.5);

    seat->weight += wl_fixed_to_double(value) * SCROLL_SENSITIVITY;
    if(seat->weight < MIN_DRAW_RADIUS)
      seat->weight = MIN_DRAW_RADIUS;

    int size = ceil(seat->weight);
    int hsize = round(size * 0.5);

    update_cursor(seat);
    wl_surface_offset(seat->wl_pointer_surface, old_hsize - hsize, old_hsize - hsize);
  }
}

static void configure_surface(void *data, struct zwlr_layer_surface_v1 *zwlr_layer_surface_v1, uint32_t serial, uint32_t width, uint32_t height)
{
  zwlr_layer_surface_v1_ack_configure(zwlr_layer_surface_v1, serial);

  struct waydraw_output *output = data;
  if(!output->snapshot)
  {
    output->snapshot = snapshot_new(width, height);
    update_output(output);
  }
}

static void release_buffer(void *data, struct wl_buffer *wl_buffer)
{
  (void)data;
  wl_buffer_destroy(wl_buffer);
}

static struct wl_buffer *create_buffer(const struct waydraw *waydraw, uint32_t width, uint32_t height, void *data)
{
  uint32_t size = width * height * 4;

  int fd = allocate_shm_file(size);
  if(fd < 0)
  {
    fprintf(stderr, "error: failed to open shm file: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  void *storage = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(storage == MAP_FAILED)
  {
    fprintf(stderr, "error: failed to mmap shm file: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  memcpy(storage, data, size);

  struct wl_shm_pool *wl_shm_pool = wl_shm_create_pool(waydraw->wl_shm, fd, size);
  struct wl_buffer *wl_buffer = wl_shm_pool_create_buffer(wl_shm_pool, 0, width, height, width * 4, WL_SHM_FORMAT_ARGB8888);
  wl_buffer_add_listener(wl_buffer, &wl_buffer_listener, NULL);

  wl_shm_pool_destroy(wl_shm_pool);
  munmap(storage, size);
  close(fd);

  return wl_buffer;
}

int main(void)
{
  struct waydraw waydraw = {0};

  wl_list_init(&waydraw.outputs);

  waydraw.seat.weight = 10;
  waydraw.seat.color_index = 0;
  waydraw.seat.mode = WAYDRAW_MODE_BRUSH;

  waydraw.seat.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if(!waydraw.seat.xkb_context)
  {
    fprintf(stderr, "error: failed to create xkb context\n");
    exit(EXIT_FAILURE);
  }

  waydraw.wl_display = wl_display_connect(NULL);
  if(!waydraw.wl_display)
  {
    fprintf(stderr, "error: failed to connect to wayland display\n");
    exit(EXIT_FAILURE);
  }

  struct wl_registry *registry = wl_display_get_registry(waydraw.wl_display);
  if(!registry)
  {
    fprintf(stderr, "error: failed to get wayland registry\n");
    exit(EXIT_FAILURE);
  }

  wl_registry_add_listener(registry, &wl_registry_listener, &waydraw);
  wl_display_roundtrip(waydraw.wl_display);

  if(!waydraw.wl_compositor)
  {
    fprintf(stderr, "error: missing wl_compositor\n");
    exit(EXIT_FAILURE);
  }

  if(!waydraw.wl_shm)
  {
    fprintf(stderr, "error: missing ml_shm\n");
    exit(EXIT_FAILURE);
  }

  if(!waydraw.zwlr_layer_shell_v1)
  {
    fprintf(stderr, "error: missing zwlr_layer_shell_v1\n");
    exit(EXIT_FAILURE);
  }

  waydraw.initialized = true;

  struct waydraw_output *output;
  wl_list_for_each(output, &waydraw.outputs, link)
    init_output(output);

  while(wl_display_dispatch(waydraw.wl_display))
    ;

  wl_display_disconnect(waydraw.wl_display);
  return 0;
}

