#include "render.h"

#include "../io/file.h"
#include "Windows.h"

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

LoadedBitmap load_bitmap(char *filename) {
  LoadedFile file = win32_load_file(filename);
  assert(file.size > 0);
  BitmapHeader *header = (BitmapHeader *)file.memory;
  u32 *pixels = (u32 *)((char *)file.memory + header->bitmap_offset);
  LoadedBitmap result = {.memory = pixels,
                         .width = header->width,
                         .height = header->height,
                         .pitch = header->width * (header->bits_per_pixel / 8)};

  // There are multiple kinds of bitmap compression. We're only going to handle
  // one kind right now, which is an uncompressed bitmap. For more info:
  // https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapv4header
  assert(header->compression == 3);

  u32 red_mask = header->red_mask;
  u32 green_mask = header->green_mask;
  u32 blue_mask = header->blue_mask;
  u32 alpha_mask = ~(red_mask | green_mask | blue_mask);

  u32 red_index = bitscan_forward(red_mask);
  u32 green_index = bitscan_forward(green_mask);
  u32 blue_index = bitscan_forward(blue_mask);
  u32 alpha_index = bitscan_forward(alpha_mask);

  u32 *source = pixels;
  for (int y = 0; y < result.height; y++) {
    for (int x = 0; x < result.width; x++) {
      u32 color = *source;

      float red_value = (color & red_mask) >> red_index;
      float green_value = (color & green_mask) >> green_index;
      float blue_value = (color & blue_mask) >> blue_index;
      float alpha_value = (color & alpha_mask) >> alpha_index;

      red_value /= 255.0f;
      green_value /= 255.0f;
      blue_value /= 255.0f;
      alpha_value /= 255.0f;

      red_value *= alpha_value;
      green_value *= alpha_value;
      blue_value *= alpha_value;

      red_value *= 255.0f;
      green_value *= 255.0f;
      blue_value *= 255.0f;
      alpha_value *= 255.0f;

      *source++ = (((u32)alpha_value << 24) | ((u32)red_value << 16) |
                   ((u32)green_value << 8) | ((u32)blue_value));
    }
  }
  return result;
}

void win32_display_buffer_in_window(Win32Buffer *buffer, HDC hdc,
                                    int windowWidth, int windowHeight) {
  int marginX = 10;
  int marginY = 10;

  // Draw black rectangles around the edges of the buffer to add "margin"
  PatBlt(hdc, 0, 0, windowWidth, marginY, BLACKNESS);
  PatBlt(hdc, 0, marginY + buffer->bitmap.height, windowWidth, windowHeight,
         BLACKNESS);
  PatBlt(hdc, 0, 0, marginX, windowHeight, BLACKNESS);
  PatBlt(hdc, marginX + buffer->bitmap.width, 0, windowWidth, windowHeight,
         BLACKNESS);

  StretchDIBits(hdc, marginX, marginY, buffer->bitmap.width,
                buffer->bitmap.height, 0, 0, buffer->bitmap.width,
                buffer->bitmap.height, buffer->bitmap.memory, &buffer->info,
                DIB_RGB_COLORS, SRCCOPY);
}

Dim win32_get_window_dimensions(HWND window) {
  RECT clientRect;
  GetClientRect(window, &clientRect);
  Dim result = {
      .height = clientRect.bottom - clientRect.top,
      .width = clientRect.right - clientRect.left,
  };
  return result;
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
