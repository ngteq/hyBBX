#include "ardop_crc.h"

/* Algorithm from ARDOPC (KN6KB / G8BPQ) — CRC-16-CCITT variant, poly 0x8810. */

unsigned int ardop_crc16(const unsigned char *data, unsigned short length)
{
    int int_register = 0xffff;
    int i;
    int j;
    int bit;
    const int int_poly = 0x8810;

    if (data == NULL || length == 0) {
        return 0xffffu;
    }

    for (j = 0; j < (int)length; j++) {
        int mask = 0x80;

        for (i = 0; i < 8; i++) {
            bit = data[j] & mask;
            mask >>= 1;

            if (int_register & 0x8000) {
                if (bit) {
                    int_register = 0xffff & (1 + (int_register << 1));
                } else {
                    int_register = 0xffff & (int_register << 1);
                }
                int_register = int_register ^ int_poly;
            } else {
                if (bit) {
                    int_register = 0xffff & (1 + (int_register << 1));
                } else {
                    int_register = 0xffff & (int_register << 1);
                }
            }
        }
    }

    return (unsigned int)int_register;
}

int ardop_crc16_valid(const unsigned char *data, unsigned short length)
{
    unsigned int crc;

    if (data == NULL || length < 3) {
        return 0;
    }

    crc = ardop_crc16(data, (unsigned short)(length - 2));
    return data[length - 2] == (unsigned char)(crc >> 8) &&
           data[length - 1] == (unsigned char)(crc & 0xff);
}

size_t ardop_crc16_append(unsigned char *data, size_t length, size_t cap)
{
    unsigned int crc;

    if (data == NULL || length + 2 > cap) {
        return 0;
    }

    crc = ardop_crc16(data, (unsigned short)length);
    data[length] = (unsigned char)(crc >> 8);
    data[length + 1] = (unsigned char)(crc & 0xff);
    return length + 2;
}
