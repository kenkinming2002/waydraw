#ifndef LIST_H
#define LIST_H

// A more elaborate intrusive doubly-linked list implementation that supported
// more use cases.
//
// In a lot of implementation, an intrusive doubly-linked is implemented as a
// circular doubly-linked list together with an empty sentinel node. The
// requirement of a sentinel node can sometimes be an annoyance.
//
// This implementation of intrusive doubly-linked list is non-circular. Namely,
// the prev link of the first node and the next link of the last node will point
// to NULL instead of to a sentinel node. To keep track of the first and last
// node of a linked list, an optional struct list * argument can be passed to
// relevant function.
//
// There is no need to initiailize struct list and struct list_link other than
// simple zero initialization.

struct list_link
{
  struct list_link *prev;
  struct list_link *next;
};

struct list
{
  struct list_link *first;
  struct list_link *last;
};

// Unlink an element from a list. If list is non-NULL, also update its first and
// last link.
void list_unlink(struct list_link *elem, struct list *list);

// Insert an element to the beginning of a list. The element elem need not be
// initialized.
void list_insert_first(struct list_link *elem, struct list *list);

// Insert an element to the end of a list. The element elem need not be
// initialized.
void list_insert_last(struct list_link *elem, struct list *list);

#endif // LIST_H
