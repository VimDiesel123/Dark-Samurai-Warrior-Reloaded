#include "./file.h"

#include <Windows.h>
#include <assert.h>

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
