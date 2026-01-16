#ifndef SNAPSHOT_H
#define SNAPSHOT_H

// Implementation of basic image drawing functionalities such as:
//  - image drawing (that is obvious)
//  - undo/redo command
//  - earlier/later command
// on top of the cario library.
//
// This is implemented internally as intrusive data structure where each element
// participate in two container:
//   - a linked list to support earlier/later command
//   - a tree to support undo/redo command

#include "list.h"

#include <cairo.h>

#include <wayland-util.h>

#include <stdint.h>
#include <stddef.h>

//enum snapshot_style
//{
//  SNAPSHOT_STYLE_SOLID,
//  SNAPSHOT_STYLE_DOTTED,
//};
//
//struct snapshot_color
//{
//  double r, g, b, a;
//};
//
//struct snapshot_point
//{
//  double x, y;
//};
//
//struct snapshot_stroke
//{
//  enum snapshot_style style;
//  struct snapshot_color color;
//
//  struct snapshot_point *items;
//  size_t count;
//  size_t capacity;
//};

struct snapshot_node
{
  struct wl_list link;

  struct snapshot_node *parent;
  struct wl_list childs;
  struct wl_list silbing_link;

  cairo_surface_t *cairo_surface;
};

struct snapshot
{
  struct wl_list nodes; // list of nodes in chronological order
  struct snapshot_node *current; // current node we will act on
};

struct snapshot *snapshot_new(uint32_t width, uint32_t height);

void snapshot_map(struct snapshot *snapshot, uint32_t *width, uint32_t *height, uint32_t **data);

void snapshot_push(struct snapshot *snapshot);
void snapshot_maybe_push(struct snapshot *snapshot);

void snapshot_undo(struct snapshot *snapshot);
void snapshot_redo(struct snapshot *snapshot);

void snapshot_earlier(struct snapshot *snapshot);
void snapshot_later(struct snapshot *snapshot);


#endif // SNAPSHOT_H
