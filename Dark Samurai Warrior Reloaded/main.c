#include <intrin.h>
#include <stdbool.h>
#include <stdio.h>
#include <windows.h>

#include "common.h"
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

typedef struct Input {
  bool leftEndedDown;
  bool rightEndedDown;
  bool upEndedDown;
  bool downEndedDown;
  bool tabEndedDown;
  DWORD lastInputTime;
  float seconds_per_frame;
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
// NOTE: This is a value that tells you how often Windows queries performance
// counters. It is determined at system boot and never changes, so it only needs
// to be set once at startup. Read more here:
// https://learn.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancefrequency
static u32 performance_frequency;

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

Glyph win32_get_glyph(Font *font, char *font_name, u32 code_point) {
  Glyph result = {.bitmap = (LoadedBitmap *)malloc(sizeof(LoadedBitmap))};
  int max_width = 256;
  int max_height = 256;
  HDC dc = 0;
  TEXTMETRIC text_metric = {0};
  HBITMAP bitmap = 0;
  HFONT hfont = 0;
  VOID *bits = 0;
  if (!dc) {
    hfont = CreateFontA(32, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                        DEFAULT_PITCH | FF_DONTCARE, font_name);
    assert(hfont);
    dc = CreateCompatibleDC(GetDC(0));
    BITMAPINFO info = {
        .bmiHeader =
            {
                .biSize = sizeof(info.bmiHeader),
                .biWidth = max_width,
                .biHeight = max_height,
                .biPlanes = 1,
                .biBitCount = 32,
                .biCompression = BI_RGB,
            },
    };
    bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, 0, 0);
    SelectObject(dc, bitmap);
    SelectObject(dc, hfont);
    SetBkColor(dc, RGB(0, 0, 0));
    GetTextMetrics(dc, &text_metric);
  }

  SIZE size;
  wchar_t cheese_point = (wchar_t)code_point;
  assert(GetTextExtentPoint32W(dc, &cheese_point, 1, &size));
  int width = MIN(size.cx, max_width);
  int height = MIN(size.cy, max_height);
  // TODO: Specify color
  SetTextColor(dc, RGB(255, 255, 255));
  assert(TextOut(dc, 0, 0, &cheese_point, 1));

  int max_x = INT16_MIN;
  int max_y = INT16_MIN;
  int min_x = INT16_MAX;
  int min_y = INT16_MAX;

  // NOTE: Preadvancing by max_height - 1 because I want to start at the last
  // row of the bitmap and go up with each iteration of y.
  u32 *row = (u32 *)bits + (max_height - 1) * max_width;
  // We drew the glyph in a bitmap that is too large for it, now we find the
  // bounding box for the non-zero pixel values.
  for (int y = 0; y < height; y++) {
    u32 *pixel = row;
    for (int x = 0; x < width; x++) {
      if (*pixel != 0) {
        max_x = max(max_x, x);
        max_y = max(max_y, y);
        min_x = min(min_x, x);
        min_y = min(min_y, y);
      }
      pixel++;
    }
    row -= max_width;
  }

  width = max_x - min_x + 1;
  height = max_y - min_y + 1;

  result.bitmap->width = width;
  result.bitmap->height = height;
  result.bitmap->pitch = result.bitmap->width * BYTES_PER_PIXEL;
  size_t bitmap_size = width * height * BYTES_PER_PIXEL;
  result.bitmap->memory = malloc(bitmap_size);
  memset(result.bitmap->memory, 0, sizeof(bitmap_size));

  char *dest_row =
      (char *)result.bitmap->memory + (height - 1) * result.bitmap->pitch;
  for (int y = min_y; y <= max_y; y++) {
    u32 *dest = (u32 *)dest_row;
    for (int x = min_x; x <= max_x; x++) {
      // TODO: cleartype antialiasing
      COLORREF pixel = GetPixel(dc, x, y);
      u32 alpha = ((pixel >> 16) & 0xFF);

      u32 color = ((alpha << 24) | (alpha << 16) | (alpha << 8) | (alpha << 0));
      *dest++ = color;
    }
    dest_row -= result.bitmap->pitch;
  }

  // TODO: Support non mono-spaced fonts. To do this I believe I will need to
  // extract the entire kerning table for the font and store it in some kind of
  // hash table or array of pairs to look up later when drawing the fonts.
  ABC abc;
  assert(GetCharABCWidthsA(dc, code_point, code_point, &abc));

  if (!font->advance_width)
    font->advance_width = abc.abcA + abc.abcB + abc.abcC;
  if (!font->line_gap)
    font->line_gap = text_metric.tmHeight + text_metric.tmExternalLeading;

  result.ascent = max_y - (size.cy - text_metric.tmDescent);

  DeleteObject(bitmap);
  DeleteObject(hfont);
  if (dc) {
    DeleteDC(dc);
    dc = 0;
  }

  return result;
}

Font win32_load_font(char *font_name) {
  Font result = {0};
  // TODO: I am only loading the "drawable" ASCII characters.
  for (char c = '!'; c < '~'; c++) {
    result.glyphs[c] = win32_get_glyph(&result, font_name, (wchar_t)(c));
  }
  return result;
}

inline void win32_initialize_performance_frequency() {
  LARGE_INTEGER performance_frequency_result;
  QueryPerformanceFrequency(&performance_frequency_result);
  performance_frequency = performance_frequency_result.QuadPart;
}

inline LARGE_INTEGER win32_get_wall_clock() {
  LARGE_INTEGER result;
  QueryPerformanceCounter(&result);
  return result;
}

inline float win32_get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end) {
  float result =
      ((float)(end.QuadPart - start.QuadPart) / (float)performance_frequency);
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

  win32_initialize_performance_frequency();

  Font test_font = win32_load_font("Consolas");

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

  float target_seconds_per_frame = 1.0f / 60.0f;

  LARGE_INTEGER last_counter = win32_get_wall_clock();

  UINT desired_scheduler_ms = 1;
  bool sleep_is_granular =
      timeBeginPeriod(desired_scheduler_ms) == TIMERR_NOERROR;

  State state = OVERWORLD;

  NPC tim = {
      .Name = "Tim", .x = 400, .y = 300, .color = v4(0.55f, 0.25f, 0.8f, 1.0f)};
  Player player = {.x = 200, .y = 200, .speed = 20};
  LoadedBitmap guy_bmp = load_bitmap("..\\assets\\guy.bmp");

  int playerX = 200;
  int playerY = 200;
  int player_speed = 20;

  Input input = {0};
  while (global_running) {
    input.seconds_per_frame = target_seconds_per_frame;

    reset_input(&input);
    win32_process_messages(&input);

    LARGE_INTEGER counter = win32_get_wall_clock();

    if (input.leftEndedDown) player.x -= player.speed;
    if (input.rightEndedDown) player.x += player.speed;
    if (input.upEndedDown) player.y += player.speed;
    if (input.downEndedDown) player.y -= player.speed;
    if (input.tabEndedDown) state = state == OVERWORLD ? BATTLE : OVERWORLD;

    HDC dc = GetDC(hwnd);
    Dim dim = win32_get_window_dimensions(hwnd);

    const V4 background = state == OVERWORLD ? v4(0.5f, 0.9f, 0.6f, 1.0f)
                                             : v4(0.0f, 0.0f, 0.2f, 1.0f);

    // clear screen
    draw_rectangle(&global_backbuffer.bitmap, 0, 0, dim.width, dim.height,
                   background);

    // draw player
    draw_bitmap(&global_backbuffer.bitmap, &guy_bmp, player.x, player.y);

    draw_string(&global_backbuffer.bitmap, test_font, 100, 100,
                "sneed's feed and seed\nformerly chuck's");

    if (state == OVERWORLD) {
      // draw TIM
      draw_rectangle(&global_backbuffer.bitmap, tim.x, tim.y, 20, 20,
                     tim.color);
    }

    // TODO: This frame-rate code is still very incomplete, but it is at least
    // enforcing a frame rate for now.
    LARGE_INTEGER work_counter = win32_get_wall_clock();
    float seconds_elapsed_this_frame =
        win32_get_seconds_elapsed(counter, work_counter);
    if (seconds_elapsed_this_frame < target_seconds_per_frame) {
      DWORD ms_to_sleep = (DWORD)(
          1000.f * (target_seconds_per_frame - seconds_elapsed_this_frame));
      if (ms_to_sleep) Sleep(ms_to_sleep);
    } else {
      // TODO: Missed Framerate. Should log here or something maybe.
    }
    LARGE_INTEGER end_counter = win32_get_wall_clock();
    float frame_time =
        1000.0f * win32_get_seconds_elapsed(counter, end_counter);
    char frame_time_buffer[100];
    sprintf_s(frame_time_buffer, sizeof(frame_time_buffer),
              "Frame Time: %.2f \n", frame_time);
    OutputDebugStringA(frame_time_buffer);
    last_counter = end_counter;

    win32_display_buffer_in_window(&global_backbuffer, dc, dim.width,
                                   dim.height);
    ReleaseDC(hwnd, dc);
  }

  return 0;
}