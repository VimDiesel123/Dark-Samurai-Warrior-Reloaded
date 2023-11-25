#pragma once
#include <stddef.h>

typedef struct LoadedFile {
  size_t size;
  void *memory;
} LoadedFile;

LoadedFile win32_load_file(char *filename);
