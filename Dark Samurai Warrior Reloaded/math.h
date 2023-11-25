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

V4 v4(float r, float g, float b, float a);

V4 v4_mul(V4 v4, float scaler);

V4 v4_add(V4 a, V4 b);