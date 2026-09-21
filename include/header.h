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

#define P7_SYN_MASK 0x01
#define P7_ACK_MASK 0x02
#define P7_FIN_MASK 0x04
#define P7_RST_MASK 0x08

typedef struct {
  uint32_t seq_num;
  uint32_t ack_num;
  uint16_t checksum;
  uint8_t flags;
} p7_header_t;

int p7_serialize_header(const p7_header_t *header, uint8_t *buffer,
                        size_t buffer_size);
int p7_deserialize_header(p7_header_t *header, const uint8_t *buffer,
                          size_t buffer_size);

#endif // HEADER_H
