#pragma once
#include "../render.h"

typedef struct Glyph {
  LoadedBitmap* bitmap;
  int ascent;
} Glyph;

typedef struct Font {
  Glyph glyphs[128];
  int advance_width;
  int line_gap;
} Font;

void draw_string(LoadedBitmap* buffer, Font* font, u32 x, u32 y, char* string);

Font win32_load_font(char* font_name);
