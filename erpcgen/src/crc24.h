#pragma once
#include <iostream>

#define CRC24_INIT 0xB704CEL
#define CRC24_POLY 0x1864CFBL

typedef long crc24;
crc24 crc24_decode(const char *string, size_t len);
crc24 crc24_decode(std::string);
