#pragma once
#include <cstdint>
#include <cmath>

namespace noise2d {

inline float fade(float t) {
  return t * t * t * (t * (t * 6 - 15) + 10);
}

inline float lerp(float a, float b, float t) {
  return a + t * (b - a);
}

inline float grad(int32_t hash, float x, float y) {
  // 8 gradients
  switch (hash & 7) {
    case 0: return  x + y;
    case 1: return -x + y;
    case 2: return  x - y;
    case 3: return -x - y;
    case 4: return  x;
    case 5: return -x;
    case 6: return  y;
    default: return -y;
  }
}

class Perlin2D {
public:
  explicit Perlin2D(uint32_t seed = 0x12345678u) {
    init(seed);
  }

  float noise(float x, float y) const {
    int32_t X = static_cast<int32_t>(std::floor(x)) & 255;
    int32_t Y = static_cast<int32_t>(std::floor(y)) & 255;

    float xf = x - std::floor(x);
    float yf = y - std::floor(y);

    float u = fade(xf);
    float v = fade(yf);

    int32_t aa = p[p[X] + Y];
    int32_t ab = p[p[X] + Y + 1];
    int32_t ba = p[p[X + 1] + Y];
    int32_t bb = p[p[X + 1] + Y + 1];

    float x1 = lerp(grad(aa, xf, yf),     grad(ba, xf - 1, yf),     u);
    float x2 = lerp(grad(ab, xf, yf - 1), grad(bb, xf - 1, yf - 1), u);
    return lerp(x1, x2, v); // range approx [-1,1]
  }

  float noise01(float x, float y) const {
    return (noise(x, y) + 1.0f) * 0.5f;
  }

private:
  int32_t p[512];

  void init(uint32_t seed) {
    uint8_t perm[256];
    for (int i = 0; i < 256; ++i) perm[i] = static_cast<uint8_t>(i);

    // simple LCG shuffle
    auto next = [&seed]() {
      seed = seed * 1664525u + 1013904223u;
      return seed;
    };

    for (int i = 255; i > 0; --i) {
      uint32_t r = next() % (i + 1);
      uint8_t tmp = perm[i];
      perm[i] = perm[r];
      perm[r] = tmp;
    }

    for (int i = 0; i < 256; ++i) {
      p[i] = perm[i];
      p[i + 256] = perm[i];
    }
  }
};

} // namespace noise2d