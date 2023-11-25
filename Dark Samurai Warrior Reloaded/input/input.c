#include "./input.h"

#include <windowsx.h>

#include "../common.h"

#define DEBOUNCE_DELAY 100

void reset_input(Input *input) {
  input->downEndedDown = 0;
  input->leftEndedDown = 0;
  input->rightEndedDown = 0;
  input->upEndedDown = 0;
  input->tabEndedDown = 0;
}

void win32_handle_key_input(MSG *msg, Input *input) {
  u32 keyCode = msg->wParam;
  bool wasDown = (msg->lParam & (1 << 30) != 0);
  bool isDown = (msg->lParam & (1 << 31) == 0);

  if (wasDown == isDown) return;

  DWORD currentTime = GetTickCount();

  if ((currentTime - input->lastInputTime) < DEBOUNCE_DELAY) return;

  switch (keyCode) {
    case VK_LEFT: {
      input->leftEndedDown = true;
    } break;
    case VK_RIGHT: {
      input->rightEndedDown = true;
    } break;
    case VK_DOWN: {
      input->downEndedDown = true;
    } break;
    case VK_UP: {
      input->upEndedDown = true;
    } break;
    case VK_TAB: {
      input->tabEndedDown = true;
    } break;
  }

  input->lastInputTime = currentTime;
}

win32_handle_mouse_move(MSG *msg, Input *input) {
  int xPos = GET_X_LPARAM(msg->lParam);
  int yPos = GET_Y_LPARAM(msg->lParam);

  input->mouseInput.pos.x = xPos;
  input->mouseInput.pos.y = yPos;
  input->mouseInput.up = false;
}

win32_handle_mouse_down(MSG *msg, Input *input) {
  input->mouseInput.down = msg->wParam == MK_LBUTTON;
  input->mouseInput.up = false;
}

win32_handle_mouse_up(MSG *msg, Input *input) {
  input->mouseInput.down = false;
  input->mouseInput.up = true;
}

void win32_process_messages(Input *input) {
  MSG msg = {0};
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    switch (msg.message) {
      case WM_KEYDOWN:
      case WM_KEYUP: {
        win32_handle_key_input(&msg, input);
      } break;
      case WM_MOUSEMOVE: {
        win32_handle_mouse_move(&msg, input);
        break;
      }
      case WM_LBUTTONDOWN: {
        win32_handle_mouse_down(&msg, input);
        break;
      }
      case WM_LBUTTONUP: {
        win32_handle_mouse_up(&msg, input);
        break;
      }
      default: {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      } break;
    }
  }
}
