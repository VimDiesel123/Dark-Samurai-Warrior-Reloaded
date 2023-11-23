#pragma once
typedef struct LoadedBitmap {
  int width;
  int height;
  int pitch;
  void *memory;
} LoadedBitmap;

typedef struct Glyph {
  LoadedBitmap *bitmap;
  int advance_width;
  int ascent;
  int x_pre_step;
} Glyph;

typedef struct Font {
  Glyph glyphs[128];
} Font;