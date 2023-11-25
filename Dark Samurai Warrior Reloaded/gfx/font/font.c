#include "./font.h"

#include <Windows.h>

Glyph win32_get_glyph(Font *font, char *font_name, u32 code_point) {
  Glyph result = {.bitmap = (LoadedBitmap *)malloc(sizeof(LoadedBitmap))};
  int max_width = 256;
  int max_height = 256;
  HDC dc = 0;
  TEXTMETRIC text_metric = {0};
  HBITMAP bitmap = 0;
  HFONT hfont = 0;
  VOID *bits = 0;
  if (!dc) {
    hfont = CreateFontA(32, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                        DEFAULT_PITCH | FF_DONTCARE, font_name);
    assert(hfont);
    dc = CreateCompatibleDC(GetDC(0));
    BITMAPINFO info = {
        .bmiHeader =
            {
                .biSize = sizeof(info.bmiHeader),
                .biWidth = max_width,
                .biHeight = max_height,
                .biPlanes = 1,
                .biBitCount = 32,
                .biCompression = BI_RGB,
            },
    };
    bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, 0, 0);
    SelectObject(dc, bitmap);
    SelectObject(dc, hfont);
    SetBkColor(dc, RGB(0, 0, 0));
    GetTextMetrics(dc, &text_metric);
  }

  SIZE size;
  wchar_t cheese_point = (wchar_t)code_point;
  assert(GetTextExtentPoint32W(dc, &cheese_point, 1, &size));
  int width = MIN(size.cx, max_width);
  int height = MIN(size.cy, max_height);
  // TODO: Specify color
  SetTextColor(dc, RGB(255, 255, 255));
  assert(TextOut(dc, 0, 0, &cheese_point, 1));

  int max_x = INT16_MIN;
  int max_y = INT16_MIN;
  int min_x = INT16_MAX;
  int min_y = INT16_MAX;

  // NOTE: Preadvancing by max_height - 1 because I want to start at the last
  // row of the bitmap and go up with each iteration of y.
  u32 *row = (u32 *)bits + (max_height - 1) * max_width;
  // We drew the glyph in a bitmap that is too large for it, now we find the
  // bounding box for the non-zero pixel values.
  for (int y = 0; y < height; y++) {
    u32 *pixel = row;
    for (int x = 0; x < width; x++) {
      if (*pixel != 0) {
        max_x = max(max_x, x);
        max_y = max(max_y, y);
        min_x = min(min_x, x);
        min_y = min(min_y, y);
      }
      pixel++;
    }
    row -= max_width;
  }

  width = max_x - min_x + 1;
  height = max_y - min_y + 1;

  result.bitmap->width = width;
  result.bitmap->height = height;
  result.bitmap->pitch = result.bitmap->width * BYTES_PER_PIXEL;
  size_t bitmap_size = width * height * BYTES_PER_PIXEL;
  result.bitmap->memory = malloc(bitmap_size);
  memset(result.bitmap->memory, 0, sizeof(bitmap_size));

  char *dest_row =
      (char *)result.bitmap->memory + (height - 1) * result.bitmap->pitch;
  for (int y = min_y; y <= max_y; y++) {
    u32 *dest = (u32 *)dest_row;
    for (int x = min_x; x <= max_x; x++) {
      // TODO: cleartype antialiasing
      COLORREF pixel = GetPixel(dc, x, y);
      u32 alpha = ((pixel >> 16) & 0xFF);

      u32 color = ((alpha << 24) | (alpha << 16) | (alpha << 8) | (alpha << 0));
      *dest++ = color;
    }
    dest_row -= result.bitmap->pitch;
  }

  // TODO: Support non mono-spaced fonts. To do this I believe I will need to
  // extract the entire kerning table for the font and store it in some kind of
  // hash table or array of pairs to look up later when drawing the fonts.
  ABC abc;
  assert(GetCharABCWidthsA(dc, code_point, code_point, &abc));

  if (!font->advance_width)
    font->advance_width = abc.abcA + abc.abcB + abc.abcC;
  if (!font->line_gap)
    font->line_gap = text_metric.tmHeight + text_metric.tmExternalLeading;

  result.ascent = max_y - (size.cy - text_metric.tmDescent);

  DeleteObject(bitmap);
  DeleteObject(hfont);
  if (dc) {
    DeleteDC(dc);
    dc = 0;
  }

  return result;
}

Font win32_load_font(char *font_name) {
  Font result = {0};
  // TODO: I am only loading the "drawable" ASCII characters.
  for (char c = '!'; c < '~'; c++) {
    result.glyphs[c] = win32_get_glyph(&result, font_name, (wchar_t)(c));
  }
  return result;
}

void draw_string(LoadedBitmap *buffer, Font *font, u32 x, u32 y, char *string) {
  char *c = string;
  Glyph *glyphs = font->glyphs;
  s32 current_x = x;
  s32 current_y = y;
  while (*c) {
    char character = *c;
    // TODO: handle newlines
    if (character == '\n') {
      current_y -= font->line_gap;
      current_x = x;
      c++;
      continue;
    }
    if (character >= '!' && character <= '~') {
      Glyph glyph = *(glyphs + character);
      draw_bitmap(buffer, glyph.bitmap, current_x, current_y - glyph.ascent);
    }
    current_x += font->advance_width;
    c++;
  }
}
