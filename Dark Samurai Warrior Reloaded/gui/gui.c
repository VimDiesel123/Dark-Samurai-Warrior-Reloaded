#include "gui.h"

#include <string.h>

static bool inside(V2 mousePos, V2 widgetPos, int widgetWidth,
                   int widgetHeight) {
  return mousePos.x > widgetPos.x && mousePos.x < widgetPos.x + widgetWidth &&
         mousePos.y > widgetPos.y && mousePos.y < widgetPos.y + widgetHeight;
}

static int center(const int totalSpace, const int needed) {
  return (totalSpace - needed) / 2;
}

static V2 textPositionCenter(Font font, const char* text, V2 widgetPosition,
                       int widgetWidth, int widgetHeight) {
  int xPos = MAX(0, widgetPosition.x + center(widgetWidth, textWidth(font, text)));
  int yPos = MAX(0, widgetPosition.y + center(widgetHeight, textHeight(font, text)));
  return (V2){xPos, yPos};
}

static int textWidth(Font font, const char* text) {
  size_t length = strlen(text);
  // TODO: (David) This only works because the font is monospaced;
  return length * font.advance_width;
}

static int textHeight(Font font, const char* text) {
  int currentTallestCharacter = 0;
  size_t length = strlen(text);
  for (size_t i = 0; i < length; ++i) {
    char currentCharacter = text[i];
    currentTallestCharacter = MAX(currentTallestCharacter,
                                  font.glyphs[currentCharacter].bitmap->height);
  }
  return currentTallestCharacter;
}

bool clickFinished(const UI context, u64 id) {
  return context.active == id && context.mouseButtonUp && context.focused == id;
}

bool clickStarted(const UI context, u64 id) {
  return context.focused == id && context.mouseButtonDown;
}
bool button(UI* context, u64 id, LoadedBitmap* bitmap, V2 pos, int width,
            int height, Color color, Font* font, const char* text) {
  draw_rectangle(bitmap, pos.x, pos.y, width, height, color);
  V2 textPos = textPositionCenter(*font, text, pos, width, height);
  draw_string(bitmap, font, textPos.x, textPos.y, text);

  if (inside(context->mousePos, pos, width, height))
    context->focused = id;
  else
    context->focused = 0;
  if (clickFinished(*context, id)) {
    context->active = 0;
    return true;
  } else if (clickStarted(*context, id)) {
    context->active = id;
    return false;
  }
}
