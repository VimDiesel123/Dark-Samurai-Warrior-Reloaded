#pragma once
typedef struct LoadedBitmap {
  int width;
  int height;
  int pitch;
  void *memory;
} LoadedBitmap;

typedef struct Glyph {
  LoadedBitmap *bitmap;
  int ascent;
} Glyph;

typedef struct Font {
  Glyph glyphs[128];
  int advance_width;
  int line_gap;
} Font;