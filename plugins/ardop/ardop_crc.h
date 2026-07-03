#ifndef ARDOP_CRC_H
#define ARDOP_CRC_H

#include <stddef.h>

/** ARDOP host/TNC CRC-16 (poly 0x8810, init 0xFFFF) per ARDOPC reference. */
unsigned int ardop_crc16(const unsigned char *data, unsigned short length);

/** Non-zero when @p length includes trailing 2-byte CRC that validates. */
int ardop_crc16_valid(const unsigned char *data, unsigned short length);

/** Append CRC16 (MSB, LSB) after @p length bytes; returns new length. */
size_t ardop_crc16_append(unsigned char *data, size_t length, size_t cap);

#endif /* ARDOP_CRC_H */
