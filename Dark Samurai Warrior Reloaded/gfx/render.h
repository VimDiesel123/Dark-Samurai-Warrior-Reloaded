#pragma once
#include <Windows.h>

#include "../common.h"
#include "../math.h"

typedef struct LoadedBitmap {
  int width;
  int height;
  int pitch;
  void* memory;
} LoadedBitmap;

typedef struct Dim {
  int width;
  int height;
} Dim;

#pragma pack(push, 1)
typedef struct BitmapHeader {
  u16 file_type;
  u32 file_size;
  u16 reserved1;
  u16 reserved2;
  u32 bitmap_offset;
  u32 size;
  s32 width;
  s32 height;
  u16 planes;
  u16 bits_per_pixel;
  u32 compression;
  u32 size_of_bitmap;
  s32 horz_resolution;
  s32 vert_resolution;
  u32 colors_used;
  u32 colors_important;

  u32 red_mask;
  u32 green_mask;
  u32 blue_mask;
} BitmapHeader;
#pragma pack(pop)

typedef struct Win32Buffer {
  BITMAPINFO info;
  LoadedBitmap bitmap;
} Win32Buffer;

LoadedBitmap load_bitmap(char* filename);

void draw_rectangle(LoadedBitmap* buffer, int x, int y, int width, int height,
                    V4 color);

Dim win32_get_window_dimensions(HWND window);
