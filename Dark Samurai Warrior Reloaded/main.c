#include <stdio.h>
#include <windows.h>

#define BYTES_PER_PIXEL 4
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
  global_backbuffer.pitch = global_backbuffer.width / BYTES_PER_PIXEL;
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

    HDC dc = GetDC(hwnd);
    Dim dim = win32_get_window_dimensions(hwnd);
    win32_display_buffer_in_window(&global_backbuffer, dc, dim.width, dim.height);
    ReleaseDC(hwnd, dc);
  }

  return 0;
}


