#pragma once
#include "../math.h"
// TODO: (David) including all of windows.h for DWORD
#include <Windows.h>
#include <stdbool.h>

typedef struct MouseInput {
  V2 pos;
  bool down;
  bool up;
} MouseInput;

typedef struct Input {
  bool leftEndedDown;
  bool rightEndedDown;
  bool upEndedDown;
  bool downEndedDown;
  bool tabEndedDown;
  DWORD lastInputTime;
  MouseInput mouseInput;
  float seconds_per_frame;
} Input;

void reset_input(Input *input);
