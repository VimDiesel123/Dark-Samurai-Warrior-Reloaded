#include <stdio.h>
#include <windows.h>
#include <stdint.h>

#define BYTES_PER_PIXEL 4
#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)
#define assert(expr) \
  if (!(expr)) {     \
    *(int *)0 = 0;   \
  }

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

typedef struct Color {
  float r;
  float g;
  float b;
  float a;
} Color;

Color color(float r, float g, float b, float a) {
  assert(r <= 1.0f && r >= 0.0f);
  assert(g <= 1.0f && g >= 0.0f);
  assert(b <= 1.0f && b >= 0.0f);
  assert(a <= 1.0f && a >= 0.0f);

  Color result = {0};
  result.r = r;
  result.g = g;
  result.b = b;
  result.a = a;
  return result;
}

static Win32Buffer global_backbuffer;

void win32_process_messages() {
  MSG msg = {0};
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

void win32_display_buffer_in_window(Win32Buffer *buffer, HDC hdc, int windowWidth, int windowHeight) {
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
  Dim result = {0};
  RECT clientRect;
  GetClientRect(window, &clientRect);
  result.height = clientRect.bottom - clientRect.top;
  result.width = clientRect.right - clientRect.left;
  return result;
}

void draw_rectangle(Win32Buffer *buffer, int x, int y, int width, int height, Color color) {
  // clip the rectangle to the edges of the buffer.
  int minX = MAX(0, x);
  int maxX = MAX(MIN(buffer->width, x + width), 0);
  int minY = MAX(0, y);
  int maxY = MAX(MIN(buffer->height, y + height), 0);

  char *row = ((char *)buffer->memory + (minX * BYTES_PER_PIXEL) + (minY * buffer->pitch));
  for (int y = minY; y < maxY; y++) 
  {
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

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  LRESULT result = 0;
  switch (uMsg) {
    case WM_PAINT: {
      PAINTSTRUCT ps = {0};
      HDC hdc = BeginPaint(hwnd, &ps);
      EndPaint(hwnd, &ps);
    } break;
    case WM_DESTROY: {
      PostQuitMessage(0);
    } break;
    default: {
      result = DefWindowProc(hwnd, uMsg, wParam, lParam);
    } break;
  }
  result = DefWindowProc(hwnd, uMsg, wParam, lParam);
  return result;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
  OutputDebugStringA("Hello, World!");

  Win32Buffer global_backbuffer = {0};
  global_backbuffer.width = 960;
  global_backbuffer.height = 540;
  global_backbuffer.pitch = global_backbuffer.width * BYTES_PER_PIXEL;
  BITMAPINFOHEADER header = {0};
  header.biSize = sizeof(global_backbuffer.info.bmiHeader);
  header.biWidth = global_backbuffer.width;
  header.biHeight = global_backbuffer.height;
  header.biPlanes = 1;
  header.biBitCount = BYTES_PER_PIXEL * 8;
  header.biCompression = BI_RGB;

  BITMAPINFO info = {0};
  info.bmiHeader = header;
  global_backbuffer.info = info;
  global_backbuffer.memory = VirtualAlloc(
      0, global_backbuffer.width * global_backbuffer.height * BYTES_PER_PIXEL,
      MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  assert(global_backbuffer.memory);


  const wchar_t CLASS_NAME[] = L"My Cool Window Class";

  WNDCLASS wc = {0};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = CLASS_NAME;

  RegisterClass(&wc);

  HWND hwnd =
      CreateWindowEx(0, CLASS_NAME, L"Dark Samurai Warrior Reloaded",
                     WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                     CW_USEDEFAULT, CW_USEDEFAULT,
                     NULL, NULL, hInstance, NULL);

  if (hwnd == NULL) {
    return 0;
  }

  ShowWindow(hwnd, nCmdShow);

  while (1) {
    win32_process_messages();

    draw_rectangle(&global_backbuffer, 200, 200, 30, 30, color(1.0f, 0.0f, 0.0f, 1.0f));
    HDC dc = GetDC(hwnd);
    Dim dim = win32_get_window_dimensions(hwnd);
    win32_display_buffer_in_window(&global_backbuffer, dc, dim.width, dim.height);
    ReleaseDC(hwnd, dc);
  }

  return 0;
}


