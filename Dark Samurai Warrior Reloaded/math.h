#pragma once
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

typedef struct V2 {
  float x, y;
} V2;

inline V4 v4(float r, float g, float b, float a) {
  assert(r <= 1.0f && r >= 0.0f);
  assert(g <= 1.0f && g >= 0.0f);
  assert(b <= 1.0f && b >= 0.0f);
  assert(a <= 1.0f && a >= 0.0f);

  V4 result = {.r = r, .g = g, .b = b, .a = a};
  return result;
}

inline V4 v4_mul(V4 v4, float scaler) {
  V4 result = {.x = v4.x * scaler,
               .y = v4.y * scaler,
               .z = v4.z * scaler,
               .w = v4.w * scaler};
  return result;
}

inline V4 v4_add(V4 a, V4 b) {
  V4 result = {.x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z, .w = a.w + b.w};
  return result;
}