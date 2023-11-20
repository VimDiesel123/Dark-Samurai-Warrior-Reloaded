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

V4 v4(float r, float g, float b, float a) {
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

inline V4 v4_mul(V4 v4, float scaler) {
  V4 result = {0};
  result.x = v4.x * scaler;
  result.y = v4.y * scaler;
  result.z = v4.z * scaler;
  result.w = v4.w * scaler;
  return result;
}


inline V4 v4_add(V4 a, V4 b) {
  V4 result = {0};
  result.x = a.x + b.x;
  result.y = a.y + b.y;
  result.z = a.z + b.z;
  result.w = a.w + b.w;
  return result;
}