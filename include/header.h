#ifndef HEADER_H
#define HEADER_H

#include <stddef.h>
#include <stdint.h>

#define P7_HEADER_SIZE 12

#define P7_MTU 1500
#define P7_UDP_HEADER_SIZE 8
#define P7_IP_HEADER_SIZE 20
#define P7_MSS                                                                 \
  (P7_MTU - P7_UDP_HEADER_SIZE - P7_IP_HEADER_SIZE - P7_HEADER_SIZE)

typedef struct {
  uint32_t seq_num;
  uint32_t ack_num;
  uint8_t flags;
  uint16_t checksum;
} p7_header_t;

int p7_serialize_header(const p7_header_t *header, uint8_t *buffer,
                        size_t buffer_size);

#endif // HEADER_H
