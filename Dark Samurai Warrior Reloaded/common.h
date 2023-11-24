#pragma once
#include <stdint.h>
#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)
#define assert(expr) \
  if (!(expr)) {     \
    *(int *)0 = 0;   \
  }

#define array_length(arr) (sizeof(arr) / sizeof(arr[0]))

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define s8 int8_t
#define s16 int16_t
#define s32 int32_t
#define s64 int64_t

#define BYTES_PER_PIXEL 4

inline u32 bitscan_forward(u32 mask) {
  u64 result = 0;
  _BitScanForward(&result, mask);
  return result;
}