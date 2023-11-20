#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

#define BYTES_PER_PIXEL 4
#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)
#define assert(expr) \
  if (!(expr)) {     \
    *(int *)0 = 0;   \
  }

#define DEBOUNCE_DELAY 100

typedef struct Win32Buffer {
  BITMAPINFO info;
  int width;
  int height;
  int pitch;
  void *memory;
} Win32Buffer;

typedef struct Dim {
  int width;
  int height;
} Dim;

typedef struct Input {
  bool leftEndedDown;
  bool rightEndedDown;
  bool upEndedDown;
  bool downEndedDown;
  bool tabEndedDown;
  DWORD lastInputTime;
} Input;

typedef struct Color {
  float r;
  float g;
  float b;
  float a;
} Color;

typedef enum State { OVERWORLD, BATTLE } State;

typedef struct NPC {
  int x;
  int y;
  const char *Name;
  Color color;
} NPC;

typedef struct Player {
  int x;
  int y;
  int speed;
} Player;

Color color(float r, float g, float b, float a) {
  assert(r <= 1.0f && r >= 0.0f);
  assert(g <= 1.0f && g >= 0.0f);
  assert(b <= 1.0f && b >= 0.0f);
  assert(a <= 1.0f && a >= 0.0f);

  Color result = {.r = r, .g = g, .b = b, .a = a};
  return result;
}

static Win32Buffer global_backbuffer;
static bool global_running;

void win32_handle_key_input(MSG *msg, Input *input) {
  unsigned int keyCode = msg->wParam;

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

void win32_process_messages(Input *input) {
  MSG msg = {0};
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    switch (msg.message) {
      case WM_KEYDOWN:
      case WM_KEYUP: {
        win32_handle_key_input(&msg, input);
      } break;
      default: {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      } break;
    }
  }
}

void win32_display_buffer_in_window(Win32Buffer *buffer, HDC hdc,
                                    int windowWidth, int windowHeight) {
  int marginX = 10;
  int marginY = 10;

  // Draw black rectangles around the edges of the buffer to add "margin"
  PatBlt(hdc, 0, 0, windowWidth, marginY, BLACKNESS);
  PatBlt(hdc, 0, marginY + buffer->height, windowWidth, windowHeight,
         BLACKNESS);
  PatBlt(hdc, 0, 0, marginX, windowHeight, BLACKNESS);
  PatBlt(hdc, marginX + buffer->width, 0, windowWidth, windowHeight, BLACKNESS);

  StretchDIBits(hdc, marginX, marginY, buffer->width, buffer->height, 0, 0,
                buffer->width, buffer->height, buffer->memory, &buffer->info,
                DIB_RGB_COLORS, SRCCOPY);
}

Dim win32_get_window_dimensions(HWND window) {
  RECT clientRect;
  GetClientRect(window, &clientRect);
  Dim result = {
      .height = clientRect.bottom - clientRect.top,
      .width = clientRect.right - clientRect.left,
  };
  return result;
}

void draw_rectangle(Win32Buffer *buffer, int x, int y, int width, int height,
                    Color color) {
  // clip the rectangle to the edges of the buffer.
  int minX = MAX(0, x);
  int maxX = MAX(MIN(buffer->width, x + width), 0);
  int minY = MAX(0, y);
  int maxY = MAX(MIN(buffer->height, y + height), 0);

  char *row = ((char *)buffer->memory + (minX * BYTES_PER_PIXEL) +
               (minY * buffer->pitch));
  for (int y = minY; y < maxY; y++) {
    unsigned int *pixel = (unsigned int *)row;
    for (int x = minX; x < maxX; x++) {
      int a_value = color.a * 255;
      int r_value = color.r * 255;
      int g_value = color.g * 255;
      int b_value = color.b * 255;
      *pixel++ = ((a_value << 24) | (r_value << 16) | (g_value << 8) | b_value);
    }
    row += buffer->pitch;
  }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  LRESULT result = 0;
  switch (uMsg) {
    case WM_PAINT: {
      PAINTSTRUCT ps = {0};
      HDC hdc = BeginPaint(hwnd, &ps);
      EndPaint(hwnd, &ps);
    } break;
    case WM_DESTROY: {
      global_running = false;
      PostQuitMessage(0);
    } break;
    default: {
      result = DefWindowProc(hwnd, uMsg, wParam, lParam);
    } break;
  }
  result = DefWindowProc(hwnd, uMsg, wParam, lParam);
  return result;
}

void reset_input(Input *input) {
  input->downEndedDown = 0;
  input->leftEndedDown = 0;
  input->rightEndedDown = 0;
  input->upEndedDown = 0;
  input->tabEndedDown = 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow) {
  global_running = true;

  Win32Buffer global_backbuffer = {
      .width = 960,
      .height = 540,
      .pitch = global_backbuffer.width * BYTES_PER_PIXEL,
      .memory = VirtualAlloc(
          0,
          global_backbuffer.width * global_backbuffer.height * BYTES_PER_PIXEL,
          MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE),
  };

  assert(global_backbuffer.memory);

  BITMAPINFO info = {.bmiHeader = {
                         .biSize = sizeof(global_backbuffer.info.bmiHeader),
                         .biWidth = global_backbuffer.width,
                         .biHeight = global_backbuffer.height,
                         .biPlanes = 1,
                         .biBitCount = BYTES_PER_PIXEL * 8,
                         .biCompression = BI_RGB,
                     }};

  global_backbuffer.info = info;

  WNDCLASS wc = {.lpfnWndProc = WindowProc,
                 .hInstance = hInstance,
                 .lpszClassName = L"My Cool Window Class"};

  RegisterClass(&wc);

  HWND hwnd =
      CreateWindowEx(0, wc.lpszClassName, L"Dark Samurai Warrior Reloaded",
                     WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                     CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

  if (hwnd == NULL) {
    return 0;
  }

  ShowWindow(hwnd, nCmdShow);

  State state = OVERWORLD;

  NPC tim = {.Name = "Tim",
             .x = 400,
             .y = 300,
             .color = color(0.55f, 0.25f, 0.8f, 1.0f)};
  Player player = {.x = 200, .y = 200, .speed = 20};

  Input input = {0};
  while (global_running) {
    reset_input(&input);
    win32_process_messages(&input);

    if (input.leftEndedDown) player.x -= player.speed;
    if (input.rightEndedDown) player.x += player.speed;
    if (input.upEndedDown) player.y += player.speed;
    if (input.downEndedDown) player.y -= player.speed;
    if (input.tabEndedDown) state = state == OVERWORLD ? BATTLE : OVERWORLD;

    HDC dc = GetDC(hwnd);
    Dim dim = win32_get_window_dimensions(hwnd);

    const Color background = state == OVERWORLD ? color(0.0f, 0.0f, 0.2f, 1.0f)
                                                : color(0.5f, 0.9f, 0.6f, 1.0f);

    // clear screen
    draw_rectangle(&global_backbuffer, 0, 0, dim.width, dim.height, background);

    // draw player
    draw_rectangle(&global_backbuffer, player.x, player.y, 30, 30,
                   color(1.0f, 0.0f, 0.0f, 1.0f));

    if (state == OVERWORLD) {
      // draw TIM
      draw_rectangle(&global_backbuffer, tim.x, tim.y, 20, 20, tim.color);
    }

    win32_display_buffer_in_window(&global_backbuffer, dc, dim.width,
                                   dim.height);
    ReleaseDC(hwnd, dc);
  }

  return 0;
}
