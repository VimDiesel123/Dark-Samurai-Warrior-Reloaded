#pragma once
#include <stdbool.h>

#include "../../common.h"
#include "../../math.h"
#include "../font/font.h"

typedef V4 Color;
typedef u64 ID;

typedef struct UI {
  ID focused, active;
  V2 mousePos;
  bool mouseButtonDown, mouseButtonUp;
} UI;

bool button(UI* context, ID id, LoadedBitmap* bitmap, V2 pos, int width,
            int height, Color color, Font* font, const char* text);