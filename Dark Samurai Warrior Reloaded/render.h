#pragma once
#include "common.h"
#include "math.h"

typedef struct LoadedBitmap {
  int width;
  int height;
  int pitch;
  void* memory;
} LoadedBitmap;

void draw_rectangle(LoadedBitmap* buffer, int x, int y, int width, int height,
                    V4 color);
