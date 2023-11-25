#include <intrin.h>
#include <stdbool.h>
#include <stdio.h>
#include <windows.h>

#include "common.h"
#include "gfx/gfx.h"
#include "input/input.h"
#include "io/file.h"
#include "math.h"

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

  UI ui = {0};

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

    ui.mousePos = input.mouseInput.pos;
    ui.mouseButtonDown = input.mouseInput.down;
    ui.mouseButtonUp = input.mouseInput.up;

    // TODO: (David) figure out the best way to handle y coords.
    // For now, translate y value to account for 0,0 being bottom left instead
    // of top left
    ui.mousePos.y = global_backbuffer.bitmap.height - ui.mousePos.y;

    HDC dc = GetDC(hwnd);
    Dim dim = win32_get_window_dimensions(hwnd);

    const V4 background = state == OVERWORLD ? v4(0.5f, 0.9f, 0.6f, 1.0f)
                                             : v4(0.0f, 0.0f, 0.2f, 1.0f);

    // clear screen
    draw_rectangle(&global_backbuffer.bitmap, 0, 0, dim.width, dim.height,
                   background);

    // draw UI
    V2 buttonPos = {50, 50};
    V4 buttonColor = v4(1.0, 0.0, 0.0, 1.0);
    const char *buttonText = state == OVERWORLD ? "Overworld" : "Battle";
    int buttonWidth = 150;
    int buttonHeight = 150;
    if (button(&ui, 69, &global_backbuffer.bitmap, buttonPos, buttonWidth,
               buttonHeight, buttonColor, &test_font, buttonText)) {
      // If I press this red button dawg, everybody heaven's gated.
      state = state == OVERWORLD ? BATTLE : OVERWORLD;
    }

    // draw player
    draw_bitmap(&global_backbuffer.bitmap, &guy_bmp, player.x, player.y);

    draw_string(&global_backbuffer.bitmap, &test_font, 350, 350,
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
      DWORD ms_to_sleep = (DWORD)(1000.f * (target_seconds_per_frame -
                                            seconds_elapsed_this_frame));
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