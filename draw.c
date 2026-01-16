#include "draw.h"

void draw_point(uint32_t *data, uint32_t width, uint32_t height, int32_t x, int32_t y, uint32_t color)
{
  if(x < 0 || (uint32_t)x >= width)
    return;

  if(y < 0 || (uint32_t)y >= height)
    return;

  data[width * y + x] = color;
}

void draw_circle(uint32_t *data, uint32_t width, uint32_t height, int32_t x, int32_t y, int32_t radius, int32_t thickness, uint32_t color)
{
  if(thickness > radius)
    thickness = radius;

  // Nah, it is fast enough.
  for(int32_t dy = -radius; dy <= radius; ++dy)
    for(int32_t dx = -radius; dx <= radius; ++dx)
      if((radius-thickness)*(radius-thickness) <= dx * dx  + dy * dy && dx * dx  + dy * dy <= radius * radius)
        draw_point(data, width, height, x+dx, y+dy, color);
}

void draw_sphere(uint32_t *data, uint32_t width, uint32_t height, int32_t x, int32_t y, int32_t radius, uint32_t color)
{
  for(int32_t dy = -radius; dy <= radius; ++dy)
    for(int32_t dx = -radius; dx <= radius; ++dx)
      if(dx * dx  + dy * dy <= radius * radius)
        draw_point(data, width, height, x+dx, y+dy, color);
}
