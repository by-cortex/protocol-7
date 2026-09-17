#include "header.h"
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int p7_serialize_header(const p7_header_t *header, uint8_t *buffer,
                        size_t buffer_size) {
  if (buffer_size < P7_HEADER_SIZE) {
    return -1;
  }

  size_t offset = 0;

  uint32_t seq_num_n = htonl(header->seq_num);
  memcpy(buffer + offset, &seq_num_n, sizeof(seq_num_n));
  offset += sizeof(seq_num_n);

  uint32_t ack_num_n = htonl(header->ack_num);
  memcpy(buffer + offset, &ack_num_n, sizeof(ack_num_n));
  offset += sizeof(ack_num_n);

  uint8_t flags_n = header->flags;
  buffer[offset] = flags_n;
  offset += 1;

  uint16_t checksum_n = htons(header->checksum);
  memcpy(buffer + offset, &checksum_n, sizeof(checksum_n));
  offset += sizeof(checksum_n);

  buffer[offset] = 0;
  offset += 1;

  return offset;
}

int p7_deserialize_header(p7_header_t *header, const uint8_t *buffer,
                          size_t buffer_size) {
  if (buffer_size < P7_HEADER_SIZE) {
    return -1;
  }

  size_t offset = 0;

  uint32_t seq_num_n;
  memcpy(&seq_num_n, buffer + offset, sizeof(seq_num_n));
  header->seq_num = ntohl(seq_num_n);
  offset += sizeof(seq_num_n);

  uint32_t ack_num_n;
  memcpy(&ack_num_n, buffer + offset, sizeof(ack_num_n));
  header->ack_num = ntohl(ack_num_n);
  offset += sizeof(ack_num_n);

  header->flags = buffer[offset];
  offset += 1;

  uint16_t checksum_n;
  memcpy(&checksum_n, buffer + offset, sizeof(checksum_n));
  header->checksum = ntohs(checksum_n);
  offset += sizeof(checksum_n);

  offset += 1;
  return offset;
}
