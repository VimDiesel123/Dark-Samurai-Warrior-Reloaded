#include <stdio.h>
#include <windows.h>

void win32_process_messages() {
  MSG msg = {0};
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  LRESULT result = 0;
  switch (uMsg) {
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    default:
      result = DefWindowProc(hwnd, uMsg, wParam, lParam);
      break;
  }
  result = DefWindowProc(hwnd, uMsg, wParam, lParam);
  return result;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
  OutputDebugStringA("Hello, World!");

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
  }

  return 0;
}


