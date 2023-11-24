#pragma once
#include <stdbool.h>

#include "../common.h"
#include "../math.h"
#include "../render.h"

typedef V4 Color;

typedef struct UI {
  u64 focused, active;
  V2 mousePos;
  bool mouseButtonDown, mouseButtonUp;
} UI;

bool button(UI* context, u64 id, LoadedBitmap* bitmap, V2 pos, int width,
            int height, Color color);