#ifndef PAINT_H
#define PAINT_H

#include <stdint.h>

void draw_point(uint32_t *data, uint32_t width, uint32_t height, int32_t x, int32_t y, uint32_t color);
void draw_circle(uint32_t *data, uint32_t width, uint32_t height, int32_t x, int32_t y, int32_t radius, uint32_t color);

#endif // PAINT_H
