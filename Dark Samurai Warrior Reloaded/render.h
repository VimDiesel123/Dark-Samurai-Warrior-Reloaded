#pragma once
typedef struct LoadedBitmap {
  int width;
  int height;
  int pitch;
  void *memory;
} LoadedBitmap;