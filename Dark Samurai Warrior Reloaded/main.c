#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>
#include <intrin.h>

#define BYTES_PER_PIXEL 4
#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)
#define assert(expr) \
  if (!(expr)) {     \
    *(int *)0 = 0;   \
  }

#include "math.h"

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define s8 int8_t
#define s16 int16_t
#define s32 int32_t
#define s64 int64_t



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

typedef union V4 {
  struct {
    float r;
    float g;
    float b;
    float a;
  };
  struct {
    float x;
    float y;
    float z;
    float w;
  };
} V4;

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

  V4 result = {0};
  result.r = r;
  result.g = g;
  result.b = b;
  result.a = a;
  return result;
}


static Win32Buffer global_backbuffer;
static bool global_running;

typedef struct LoadedFile {
  size_t size;
  void *memory;
} LoadedFile;

typedef struct LoadedBitmap {
  int width;
  int height;
  int pitch;
  void *memory;
} LoadedBitmap;

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

inline u32 bitscan_forward(u32 mask) {
  u64 result = 0;
  _BitScanForward(&result, mask);
  return result;
}

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
  ReadFile(file_handle, result.memory, file_size32, &bytes_read, 0);
  assert(bytes_read == file_size32);
  result.size = file_size32;

  return result;
}

LoadedBitmap load_bitmap(char* filename) { 
  LoadedBitmap result = {0};
  LoadedFile file = win32_load_file(filename);
  assert(file.size > 0);
  BitmapHeader *header = (BitmapHeader *)file.memory;
  u32 *pixels = (u32 *)((char *)file.memory + header->bitmap_offset);
  result.memory = pixels;
  result.width = header->width;
  result.height = header->height;
  result.pitch = header->width * (header->bits_per_pixel / 8);

  // There are multiple kinds of bitmap compression. We're only going to handle one kind right now, which is an uncompressed bitmap.
  // For more info: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapv4header
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

inline V4 v4_color_from_u32(u32 color) {
  V4 result = {0};
  result.r = (float)((color >> 16) & 0xFF) / 255.0f;
  result.g = (float)((color >> 8) & 0xFF) / 255.0f;
  result.b = (float)((color) & 0xFF) / 255.0f;
  result.a = (float)((color >> 24) & 0xFF) / 255.0f;
  return result;
}

inline u32 u32_color_from_v4(V4 color) { 
  V4 color255 = v4_mul(color, 255.0f);
  u32 result = (((u32)color255.a << 24) |
                ((u32)color255.r << 16) |
                ((u32)color255.g << 8) |
                ((u32)color255.b));
  return result;
}

void draw_bitmap(Win32Buffer *buffer, LoadedBitmap *bitmap, u32 pos_x, u32 pos_y) {
  char *source_row = (char *)bitmap->memory;

  char *dest_row = ((char *)buffer->memory + (pos_x * BYTES_PER_PIXEL) +
                   (pos_y * buffer->pitch));

  for (s32 y = 0; y < bitmap->height; y++) {
    u32 *source_pixel = (u32 *)source_row;
    u32 *dest_pixel = (u32 *)dest_row;
    for (s32 x = 0; x < bitmap->width; x++) {
      u32 source_color_32 = *source_pixel;
      u32 dest_color_32 = *dest_row;

      V4 source_color = v4_color_from_u32(source_color_32);
      V4 dest_color = v4_color_from_u32(dest_color_32);

      // NOTE: This is a lerp on the source_color's alpha value because the alpha has already been premultiplied.
      // lerp(a, t, b) = a * (1 - t) + b * t
      // dest_color * (1 - source_color.a) + (source_color * source_color.a) <- the source color has already had its alpha premultiplied.
      V4 result =
          v4_add(v4_mul(dest_color, (1.0f - source_color.a)), source_color);

      u32 color = u32_color_from_v4(result);
      
      *dest_pixel = color;

      dest_pixel++;
      source_pixel++;
    }
    source_row += bitmap->pitch;
    dest_row += buffer->pitch;
  }
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
                    V4 color) {
  // clip the rectangle to the edges of the buffer.
  int minX = MAX(0, x);
  int maxX = MAX(MIN(buffer->width, x + width), 0);
  int minY = MAX(0, y);
  int maxY = MAX(MIN(buffer->height, y + height), 0);

  char *row = ((char *)buffer->memory + (minX * BYTES_PER_PIXEL) +
               (minY * buffer->pitch));
  for (int y = minY; y < maxY; y++) {
    u32 *pixel = (u32 *)row;
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
  LoadedBitmap guy_bmp = load_bitmap("..\\assets\\guy.bmp");

  int playerX = 200;
  int playerY = 200;
  int player_speed = 20;

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
    draw_rectangle(&global_backbuffer, 0, 0, dim.width, dim.height,
                   v4(0.0f, 0.0f, 0.2f, 1.0f));

    // draw player
    draw_rectangle(&global_backbuffer, playerX, playerY, 30, 30,
                   v4(1.0f, 0.0f, 0.0f, 1.0f));

    draw_bitmap(&global_backbuffer, &guy_bmp, 100, 100);

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