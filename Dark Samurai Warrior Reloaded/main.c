#include <intrin.h>
#include <stdbool.h>
#include <stdio.h>
#include <windows.h>
#include <windowsx.h>

#include "common.h"
#include "gui/gui.h"
#include "math.h"
#include "render.h"

#define DEBOUNCE_DELAY 100

typedef struct Win32Buffer {
  BITMAPINFO info;
  LoadedBitmap bitmap;
} Win32Buffer;

typedef struct Dim {
  int width;
  int height;
} Dim;

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
} Input;

typedef enum State { OVERWORLD, BATTLE } State;

typedef struct NPC {
  int x;
  int y;
  const char *Name;
  V4 color;
} NPC;

typedef struct Player {
  int x;
  int y;
  int speed;
} Player;

static Win32Buffer global_backbuffer;
static bool global_running;

typedef struct LoadedFile {
  size_t size;
  void *memory;
} LoadedFile;

#pragma pack(push, 1)
typedef struct BitmapHeader {
  u16 file_type;
  u32 file_size;
  u16 reserved1;
  u16 reserved2;
  u32 bitmap_offset;
  u32 size;
  s32 width;
  s32 height;
  u16 planes;
  u16 bits_per_pixel;
  u32 compression;
  u32 size_of_bitmap;
  s32 horz_resolution;
  s32 vert_resolution;
  u32 colors_used;
  u32 colors_important;

  u32 red_mask;
  u32 green_mask;
  u32 blue_mask;
} BitmapHeader;
#pragma pack(pop)

LoadedFile win32_load_file(char *filename) {
  LoadedFile result = {0};
  HANDLE file_handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  assert(file_handle != INVALID_HANDLE_VALUE);
  LARGE_INTEGER file_size64;
  assert(GetFileSizeEx(file_handle, &file_size64) != INVALID_FILE_SIZE);
  size_t file_size32 = file_size64.QuadPart;
  result.memory =
      VirtualAlloc(0, file_size32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  DWORD bytes_read;
  assert(ReadFile(file_handle, result.memory, file_size32, &bytes_read, 0));
  assert(bytes_read == file_size32);
  result.size = file_size32;

  return result;
}

LoadedBitmap load_bitmap(char *filename) {
  LoadedFile file = win32_load_file(filename);
  assert(file.size > 0);
  BitmapHeader *header = (BitmapHeader *)file.memory;
  u32 *pixels = (u32 *)((char *)file.memory + header->bitmap_offset);
  LoadedBitmap result = {.memory = pixels,
                         .width = header->width,
                         .height = header->height,
                         .pitch = header->width * (header->bits_per_pixel / 8)};

  // There are multiple kinds of bitmap compression. We're only going to handle
  // one kind right now, which is an uncompressed bitmap. For more info:
  // https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapv4header
  assert(header->compression == 3);

  u32 red_mask = header->red_mask;
  u32 green_mask = header->green_mask;
  u32 blue_mask = header->blue_mask;
  u32 alpha_mask = ~(red_mask | green_mask | blue_mask);

  u32 red_index = bitscan_forward(red_mask);
  u32 green_index = bitscan_forward(green_mask);
  u32 blue_index = bitscan_forward(blue_mask);
  u32 alpha_index = bitscan_forward(alpha_mask);

  u32 *source = pixels;
  for (int y = 0; y < result.height; y++) {
    for (int x = 0; x < result.width; x++) {
      u32 color = *source;

      // TODO: Premultiply alpha
      float red_value = (color & red_mask) >> red_index;
      float green_value = (color & green_mask) >> green_index;
      float blue_value = (color & blue_mask) >> blue_index;
      float alpha_value = (color & alpha_mask) >> alpha_index;

      red_value /= 255.0f;
      green_value /= 255.0f;
      blue_value /= 255.0f;
      alpha_value /= 255.0f;

      red_value *= alpha_value;
      green_value *= alpha_value;
      blue_value *= alpha_value;

      red_value *= 255.0f;
      green_value *= 255.0f;
      blue_value *= 255.0f;
      alpha_value *= 255.0f;

      *source++ = (((u32)alpha_value << 24) | ((u32)red_value << 16) |
                   ((u32)green_value << 8) | ((u32)blue_value));
    }
  }
  return result;
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

win32_handle_mouse_up(MSG* msg, Input* input){
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
      case WM_LBUTTONDOWN:{
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

void win32_display_buffer_in_window(Win32Buffer *buffer, HDC hdc,
                                    int windowWidth, int windowHeight) {
  int marginX = 10;
  int marginY = 10;

  // Draw black rectangles around the edges of the buffer to add "margin"
  PatBlt(hdc, 0, 0, windowWidth, marginY, BLACKNESS);
  PatBlt(hdc, 0, marginY + buffer->bitmap.height, windowWidth, windowHeight,
         BLACKNESS);
  PatBlt(hdc, 0, 0, marginX, windowHeight, BLACKNESS);
  PatBlt(hdc, marginX + buffer->bitmap.width, 0, windowWidth, windowHeight,
         BLACKNESS);

  StretchDIBits(hdc, marginX, marginY, buffer->bitmap.width,
                buffer->bitmap.height, 0, 0, buffer->bitmap.width,
                buffer->bitmap.height, buffer->bitmap.memory, &buffer->info,
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
      .bitmap.width = 960,
      .bitmap.height = 540,
      .bitmap.pitch = global_backbuffer.bitmap.width * BYTES_PER_PIXEL,
      .bitmap.memory =
          VirtualAlloc(0,
                       global_backbuffer.bitmap.width *
                           global_backbuffer.bitmap.height * BYTES_PER_PIXEL,
                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE),
  };

  BITMAPINFO info = {.bmiHeader = {
                         .biSize = sizeof(global_backbuffer.info.bmiHeader),
                         .biWidth = global_backbuffer.bitmap.width,
                         .biHeight = global_backbuffer.bitmap.height,
                         .biPlanes = 1,
                         .biBitCount = BYTES_PER_PIXEL * 8,
                         .biCompression = BI_RGB,
                     }};
  global_backbuffer.info = info;
  global_backbuffer.bitmap.memory =
      VirtualAlloc(0,
                   global_backbuffer.bitmap.width *
                       global_backbuffer.bitmap.height * BYTES_PER_PIXEL,
                   MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  assert(global_backbuffer.bitmap.memory);

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

  NPC tim = {
      .Name = "Tim", .x = 400, .y = 300, .color = v4(0.55f, 0.25f, 0.8f, 1.0f)};
  Player player = {.x = 200, .y = 200, .speed = 20};
  LoadedBitmap guy_bmp = load_bitmap("..\\assets\\guy.bmp");

  int playerX = 200;
  int playerY = 200;
  int player_speed = 20;

  UI ui = {0};

  Input input = {0};
  while (global_running) {
    reset_input(&input);
    win32_process_messages(&input);

    if (input.leftEndedDown) player.x -= player.speed;
    if (input.rightEndedDown) player.x += player.speed;
    if (input.upEndedDown) player.y += player.speed;
    if (input.downEndedDown) player.y -= player.speed;
    if (input.tabEndedDown) state = state == OVERWORLD ? BATTLE : OVERWORLD;

    ui.mousePos = input.mouseInput.pos;
    ui.mouseButtonDown = input.mouseInput.down;
    ui.mouseButtonUp = input.mouseInput.up;

    // TODO: (David) figure out the best way to handle y coords. 
    // For now, translate y value to account for 0,0 being bottom left instead of top left
    ui.mousePos.y = global_backbuffer.bitmap.height - ui.mousePos.y;

    HDC dc = GetDC(hwnd);
    Dim dim = win32_get_window_dimensions(hwnd);

    const V4 background = state == OVERWORLD ? v4(0.0f, 0.0f, 0.2f, 1.0f)
                                             : v4(0.5f, 0.9f, 0.6f, 1.0f);

    // clear screen
    draw_rectangle(&global_backbuffer.bitmap, 0, 0, dim.width, dim.height,
                   background);

    // draw UI
    V2 buttonPos = { 50, 50};
    if(button(&ui, 69, &global_backbuffer.bitmap, buttonPos, 100, 100, v4(1.0, 0.0, 0.0, 1.0))){
      //If I click this red button dawg, everybody heaven's gated.
      state = state == OVERWORLD ? BATTLE : OVERWORLD;
    }

    // draw player
    draw_bitmap(&global_backbuffer.bitmap, &guy_bmp, player.x, player.y);

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