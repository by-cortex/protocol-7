#ifndef DEBUG_H
#define DEBUG_H

#include "header.h"
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>

#define P7_COLOR_RED "\033[0;31m"
#define P7_COLOR_YELLOW "\033[0;33m"
#define P7_COLOR_RESET "\033[0m"

typedef enum {
  P7_SEND_DIRECTION,
  P7_RECV_DIRECTION,
  P7_RETRY_DIRECTION,
  P7_DROP_DIRECTION
} P7_SEGMENT_DIRECTION;

void p7_print_hex_dump(const uint8_t *buffer, ssize_t buffer_len);
void p7_log_segment(P7_SEGMENT_DIRECTION direction, const p7_header_t *header,
                    size_t payload_len, const struct sockaddr_in *addr);

#endif // DEBUG_H
