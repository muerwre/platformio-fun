#pragma once
#include <Arduino.h>

// Minimal protobuf wire-format writer — symmetric to ProtoReader.
// Writes into a caller-provided fixed buffer; silently stops if it overflows
// (check size() vs your buffer if that matters).
class ProtoWriter
{
public:
  ProtoWriter(uint8_t *buf, size_t cap) : buf(buf), cap(cap) {}

  size_t size() const { return len; }
  const uint8_t *data() const { return buf; }

  void varint(uint32_t field, uint64_t v)
  {
    tag(field, 0);
    rawVarint(v);
  }

  void fixed32(uint32_t field, uint32_t v)
  {
    tag(field, 5);
    for (int i = 0; i < 4; i++)
      put((v >> (8 * i)) & 0xFF); // little-endian
  }

  // protobuf float = fixed32 carrying the IEEE-754 bit pattern.
  void float32(uint32_t field, float v)
  {
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));
    fixed32(field, bits);
  }

  void bytes(uint32_t field, const uint8_t *d, size_t n)
  {
    tag(field, 2);
    rawVarint(n);
    for (size_t i = 0; i < n; i++)
      put(d[i]);
  }

  void string(uint32_t field, const char *s)
  {
    bytes(field, (const uint8_t *)s, strlen(s));
  }

  // Embed a nested message (length-delimited).
  void message(uint32_t field, const ProtoWriter &sub)
  {
    bytes(field, sub.data(), sub.size());
  }

private:
  void tag(uint32_t field, uint8_t wt) { rawVarint(((uint64_t)field << 3) | wt); }

  void rawVarint(uint64_t v)
  {
    while (v >= 0x80)
    {
      put((uint8_t)(v | 0x80));
      v >>= 7;
    }
    put((uint8_t)v);
  }

  void put(uint8_t b)
  {
    if (len < cap)
      buf[len++] = b;
  }

  uint8_t *buf;
  size_t cap;
  size_t len = 0;
};
