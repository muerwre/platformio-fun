#pragma once
#include <Arduino.h>

// Minimal protobuf wire-format reader — just enough to walk Meshtastic
// FromRadio messages and pull out the fields we care about.
//
// Wire types we handle: 0 (varint), 1 (64-bit), 2 (length-delimited), 5 (32-bit).
class ProtoReader
{
public:
  ProtoReader(const uint8_t *data, size_t len) : p(data), end(data + len) {}

  bool hasMore() const { return p < end; }

  // Read the next field header. Returns false at end of buffer.
  bool nextField(uint32_t &fieldNum, uint8_t &wireType)
  {
    if (p >= end)
      return false;
    uint64_t tag = readVarint();
    fieldNum = (uint32_t)(tag >> 3);
    wireType = (uint8_t)(tag & 0x07);
    return true;
  }

  uint64_t readVarint()
  {
    uint64_t result = 0;
    int shift = 0;
    while (p < end && shift < 64)
    {
      uint8_t b = *p++;
      result |= (uint64_t)(b & 0x7F) << shift;
      if (!(b & 0x80))
        break;
      shift += 7;
    }
    return result;
  }

  // Length-delimited field: returns pointer + length, advances past it.
  const uint8_t *readBytes(size_t &outLen)
  {
    uint64_t len = readVarint();
    const uint8_t *start = p;
    if (start + len > end)
      len = end - start;
    p += len;
    outLen = (size_t)len;
    return start;
  }

  // Length-delimited field as an Arduino String.
  String readString()
  {
    size_t len;
    const uint8_t *b = readBytes(len);
    String s;
    s.reserve(len);
    for (size_t i = 0; i < len; i++)
      s += (char)b[i];
    return s;
  }

  // Skip a field whose value we don't need.
  void skip(uint8_t wireType)
  {
    switch (wireType)
    {
    case 0: readVarint(); break;                 // varint
    case 1: p += 8; break;                       // 64-bit
    case 2: { size_t l; readBytes(l); } break;   // length-delimited
    case 5: p += 4; break;                       // 32-bit
    default: p = end; break;                      // unknown -> stop
    }
  }

private:
  const uint8_t *p;
  const uint8_t *end;
};
