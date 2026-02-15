#include <Arduino.h>

uint8_t utf8Ascii(uint8_t ascii)
// Convert a single Character from UTF8 to Extended ASCII according to ISO 8859-1,
// also called ISO Latin-1. Codes 128-159 contain the Microsoft Windows Latin-1
// extended characters:
// - codes 0..127 are identical in ASCII and UTF-8
// - codes 160..191 in ISO-8859-1 and Windows-1252 are two-byte characters in UTF-8
//                 + 0xC2 then second byte identical to the extended ASCII code.
// - codes 192..255 in ISO-8859-1 and Windows-1252 are two-byte characters in UTF-8
//                 + 0xC3 then second byte differs only in the first two bits to extended ASCII code.
// - codes 128..159 in Windows-1252 are different, but usually only the €-symbol will be needed from this range.
//                 + The euro symbol is 0x80 in Windows-1252, 0xa4 in ISO-8859-15, and 0xe2 0x82 0xac in UTF-8.
//
// Modified from original code at http://playground.arduino.cc/Main/Utf8ascii
// Extended ASCII encoding should match the characters at http://www.ascii-code.com/
//
// Extended to support Cyrillic characters from RomanCyrrilic.h font (Windows-1251 layout):
// Font uses Windows-1251 encoding at positions 192-255 (0xC0-0xFF)
// - UTF-8 0xD0 0x90-0xBF: А-Я, а-п -> Windows-1251 0xC0-0xEF (add 0x30 to second byte)
// - UTF-8 0xD1 0x80-0x8F: р-я -> Windows-1251 0xF0-0xFF (add 0x70 to second byte)
// - UTF-8 0xD0 0x81: Ё -> maps to Е at 0xC5 (197)
// - UTF-8 0xD1 0x91: ё -> maps to е at 0xE5 (229)
//
// Return "0" if a byte has to be ignored.
{
  static uint8_t cPrev;
  uint8_t c = '\0';

  if (ascii < 0x7f) // Standard ASCII-set 0..0x7F, no conversion
  {
    cPrev = '\0';
    c = ascii;
  }
  else
  {
    switch (cPrev) // Conversion depending on preceding UTF8-character
    {
    case 0xC2:
      c = ascii;
      break;
    case 0xC3:
      c = ascii | 0xC0;
      break;
    case 0x82:
      if (ascii == 0xAC)
        c = 0x80; // Euro symbol special case
    case 0xE2:
      switch (ascii)
      {
      case 0x80:
        c = 133;
        break; // ellipsis special case
      }
      break;

      // Cyrillic UTF-8 support for RomanCyrrilic.h font (Windows-1251 layout at positions 192-255)
    case 0xD0:
      if (ascii >= 0x90 && ascii <= 0xBF)
        c = ascii + 0x30; // UTF-8 0xD0 0x90-0xBF -> Windows-1251 0xC0-0xEF (192-239)
      else if (ascii == 0x81)
        c = 0xC5; // Ё -> Е (Windows-1251 0xC5 = 197)
      break;

    case 0xD1:
      if (ascii >= 0x80 && ascii <= 0x8F)
        c = ascii + 0x70; // UTF-8 0xD1 0x80-0x8F -> Windows-1251 0xF0-0xFF (240-255)
      else if (ascii == 0x91)
        c = 0xE5; // ё -> е (Windows-1251 0xE5 = 229)
      break;

    default:
      break;
    }
    cPrev = ascii; // save last char
  }

  return (c);
}

void utf8Ascii(char *s)
// In place conversion UTF-8 string to Extended ASCII
// The extended ASCII string is always shorter.
{
  uint8_t c;
  char *cp = s;

  while (*s != '\0')
  {
    c = utf8Ascii(*s++);
    if (c != '\0')
      *cp++ = c;
  }
  *cp = '\0'; // terminate the new string
}