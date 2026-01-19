#include "snapshot.h"

#include "cairo-utils.h"
#include "cairo-wayland-utils.h"

#include <cairo.h>
#include <wayland-util.h>

#include <stdlib.h>

struct snapshot *snapshot_new(uint32_t width, uint32_t height)
{
  struct snapshot_node *node = calloc(1, sizeof *node);
  wl_list_init(&node->childs);
  node->cairo_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);

  struct snapshot *snapshot = calloc(1, sizeof *snapshot);
  wl_list_init(&snapshot->nodes);
  wl_list_insert(&snapshot->nodes, &node->link);

  snapshot->current = node;
  return snapshot;
}

void snapshot_push(struct snapshot *snapshot)
{
  struct snapshot_node *node = calloc(1, sizeof *node);
  wl_list_init(&node->childs);
  node->cairo_surface = cairo_image_surface_clone(snapshot->current->cairo_surface);

  wl_list_insert(snapshot->nodes.prev, &node->link);
  wl_list_insert(snapshot->current->childs.prev, &node->silbing_link);

  node->parent = snapshot->current;
  snapshot->current = node;
}

void snapshot_maybe_push(struct snapshot *snapshot)
{
  if(!wl_list_empty(&snapshot->current->childs))
    snapshot_push(snapshot);
}

void snapshot_undo(struct snapshot *snapshot)
{
  struct snapshot_node *parent = snapshot->current->parent;
  if(!parent)
    return;

  wl_list_remove(&snapshot->current->silbing_link);
  wl_list_insert(parent->childs.prev, &snapshot->current->silbing_link);

  snapshot->current = parent;
}

void snapshot_redo(struct snapshot *snapshot)
{
  if(wl_list_empty(&snapshot->current->childs))
    return;

  struct wl_list *elem = snapshot->current->childs.prev;
  struct snapshot_node *node = wl_container_of(elem, node, silbing_link);
  snapshot->current = node;
}

void snapshot_earlier(struct snapshot *snapshot)
{
  struct wl_list *elem = snapshot->current->link.prev;
  if(elem == &snapshot->nodes)
    return;

  struct snapshot_node *node = wl_container_of(elem, node, link);
  snapshot->current = node;
}

void snapshot_later(struct snapshot *snapshot)
{
  struct wl_list *elem = snapshot->current->link.next;
  if(elem == &snapshot->nodes)
    return;

  struct snapshot_node *node = wl_container_of(elem, node, link);
  snapshot->current = node;
}

void snapshot_update_wl_surface(struct snapshot *snapshot,
                                struct wl_surface *wl_surface,
                                struct wl_shm *shm)
{
  wl_surface_update_from_cairo_surface(wl_surface, snapshot->current->cairo_surface,
                                  shm);
}
