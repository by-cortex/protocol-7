#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stddef.h>
#include <stdint.h>

uint16_t p7_calculate_checksum(uint8_t *data, size_t data_size);

#endif // CHECKSUM_H
