#pragma once
#include "common.h"
#include "math.h"
#include "render.h"

inline V4 v4_color_from_u32(u32 color) {
  V4 result = {.r = (float)((color >> 16) & 0xFF) / 255.0f,
               .g = (float)((color >> 8) & 0xFF) / 255.0f,
               .b = (float)((color)&0xFF) / 255.0f,
               .a = (float)((color >> 24) & 0xFF) / 255.0f};
  return result;
}

inline u32 u32_color_from_v4(V4 color) {
  V4 color255 = v4_mul(color, 255.0f);
  u32 result = (((u32)color255.a << 24) | ((u32)color255.r << 16) |
                ((u32)color255.g << 8) | ((u32)color255.b));
  return result;
}

void draw_bitmap(LoadedBitmap *buffer, LoadedBitmap *bitmap, u32 pos_x,
                 u32 pos_y) {
  // TODO: clip the bitmap
  char *source_row = (char *)bitmap->memory;

  char *dest_row = ((char *)buffer->memory + (pos_x * BYTES_PER_PIXEL) +
                    (pos_y * buffer->pitch));

  for (s32 y = 0; y < bitmap->height; y++) {
    u32 *source_pixel = (u32 *)source_row;
    u32 *dest_pixel = (u32 *)dest_row;
    for (s32 x = 0; x < bitmap->width; x++) {
      u32 source_color_32 = *source_pixel;
      u32 dest_color_32 = *dest_pixel;

      V4 source_color = v4_color_from_u32(source_color_32);
      V4 dest_color = v4_color_from_u32(dest_color_32);

      // NOTE: This is a lerp on the source_color's alpha value because the
      // alpha has already been premultiplied. lerp(a, t, b) = a * (1 - t) + b *
      // t dest_color * (1 - source_color.a) + (source_color * source_color.a)
      // <- the source color has already had its alpha premultiplied.
      V4 result =
          v4_add(v4_mul(dest_color, (1.0f - source_color.a)), source_color);

      u32 color = u32_color_from_v4(result);

      *dest_pixel = color;

      dest_pixel++;
      source_pixel++;
    }
    source_row += bitmap->pitch;
    dest_row += buffer->pitch;
  }
}

void draw_rectangle(LoadedBitmap *buffer, int x, int y, int width, int height,
                    V4 color) {
  // clip the rectangle to the edges of the buffer.
  int minX = MAX(0, x);
  int maxX = MAX(MIN(buffer->width, x + width), 0);
  int minY = MAX(0, y);
  int maxY = MAX(MIN(buffer->height, y + height), 0);

  char *row = ((char *)buffer->memory + (minX * BYTES_PER_PIXEL) +
               (minY * buffer->pitch));
  for (int y = minY; y < maxY; y++) {
    u32 *pixel = (u32 *)row;
    for (int x = minX; x < maxX; x++) {
      int a_value = color.a * 255;
      int r_value = color.r * 255;
      int g_value = color.g * 255;
      int b_value = color.b * 255;
      *pixel++ = ((a_value << 24) | (r_value << 16) | (g_value << 8) | b_value);
    }
    row += buffer->pitch;
  }
}


void draw_string(LoadedBitmap* buffer, Font *font, u32 x, u32 y, char* string) { 
  char *c = string;
  Glyph *glyphs = font->glyphs;
  s32 current_x = x;
  s32 current_y = y;
  while (*c) {
    char character = *c;
    assert(character >= '!' && character <= '~');
    Glyph glyph = *(glyphs + character);
    current_x += glyph.x_pre_step;
    draw_bitmap(buffer, glyph.bitmap, current_x, current_y - glyph.ascent);
    current_x += glyph.advance_width;
    c++;
  }
}