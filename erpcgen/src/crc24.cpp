#include "crc24.h"


crc24 crc24_decode(const char *string, size_t len)
{
    crc24 crc = CRC24_INIT;
    int i;
    while (len--)
    {
        crc ^= (*string++) << 16;
        for (i = 0; i < 8; i++)
        {
            crc <<= 1;
            if (crc & 0x1000000)
                crc ^= CRC24_POLY;
        }
    }
    return crc & 0xFFFFFFL;
}

crc24 crc24_decode(std::string string){
    return crc24_decode(string.data(), string.length());
}