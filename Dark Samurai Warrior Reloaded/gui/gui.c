#include "gui.h"

#include <string.h>

static bool inside(V2 mousePos, V2 widgetPos, int widgetWidth,
                   int widgetHeight) {
  return mousePos.x > widgetPos.x && mousePos.x < widgetPos.x + widgetWidth &&
         mousePos.y > widgetPos.y && mousePos.y < widgetPos.y + widgetHeight;
}

static V2 textPosition(Font font, const char* text, V2 widgetPosition,
                       int widgetWidth, int widgetHeight) {
  int tw = textWidth(font, text);
  int th = textHeight(font, text);
  int xPos = MAX(0, widgetPosition.x + widgetWidth / 2 - tw / 2);
  int yPos = MAX(0, widgetPosition.y + widgetHeight / 2 - th / 2);
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
  V2 textPos = textPosition(*font, text, pos, width, height);
  draw_string(bitmap, font, textPos.x, textPos.y, text);

  bool result = false;
  if (clickFinished(*context, id)) {
    result = true;
    context->active = 0;
  } else if (clickStarted(*context, id))
    context->active = id;

  if (inside(context->mousePos, pos, width, height)) context->focused = id;

  return result;
}
