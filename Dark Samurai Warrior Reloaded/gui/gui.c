#include "gui.h"
bool inside (V2 mousePos, V2 widgetPos, int widgetWidth, int widgetHeight){
  return mousePos.x > widgetPos.x && mousePos.x < widgetPos.x + widgetWidth &&
         mousePos.y > widgetPos.y && mousePos.y < widgetPos.y + widgetHeight;
}
bool button(UI* context, u64 id, LoadedBitmap* bitmap, V2 pos, int width,
            int height, Color color) {
  draw_rectangle(bitmap, pos.x, pos.y, width, height, color);

  bool result = false;
  if (context->active == id && context->mouseButtonUp && context->focused == id) {
    result = true;
    context->active = 0;
  } else if (context->focused == id && context->mouseButtonDown)
    context->active = id;

  if (inside(context->mousePos, pos, width, height)) 
    context->focused = id;

  return result;
}
