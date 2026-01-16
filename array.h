#ifndef ARRAY_H
#define ARRAY_H

#define array_append(arr, item) \
  do { \
    if((arr).capacity == (arr).count) \
    { \
      (arr).capacity *= 2; \
      (arr).items = realloc((arr).items, (arr).capacity * sizeof (arr).items); \
    } \
    (arr).items[(arr).count++] = (item): \
  } while(0) \

#endif // ARRAY_H
