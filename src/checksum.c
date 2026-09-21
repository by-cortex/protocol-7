#include "checksum.h"

uint16_t p7_calculate_checksum(uint8_t *data, size_t data_size) {
  uint32_t sum = 0;

  for (size_t i = 0; i < data_size; i += 2) {
    uint16_t word = (data[i] << 8) | (i + 1 < data_size ? data[i + 1] : 0);
    sum += word;
  }

  while (sum >> 16) {
    sum = (sum & 0xFFFF) + (sum >> 16);
  }

  return (uint16_t)~sum;
}
