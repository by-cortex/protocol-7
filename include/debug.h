#ifndef DEBUG_H
#define DEBUG_H

#include "header.h"
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>

typedef enum { P7_SEND_DIRECTION, P7_RECV_DIRECTION } P7_SEGMENT_DIRECTION;

void p7_print_hex_dump(const uint8_t *buffer, ssize_t buffer_len);
void p7_log_segment(P7_SEGMENT_DIRECTION direction, const p7_header_t *header,
                    size_t payload_len, const struct sockaddr_in *addr);

#endif // DEBUG_H
