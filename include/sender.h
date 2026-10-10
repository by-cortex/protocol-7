#ifndef SENDER_H
#define SENDER_H

#include "args.h"
#include "header.h"
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>

#define P7_WINDOW_SIZE 32
#define P7_TIMEOUT_MS 1000

typedef struct {
  uint32_t seq_num;
  uint8_t data[P7_HEADER_SIZE + P7_MSS];
  size_t data_len;
  int is_acked;
  uint64_t send_time_ms;
} p7_sender_slot_t;

typedef struct {
  p7_sender_slot_t slots[P7_WINDOW_SIZE];
  uint32_t base;
  uint32_t new_seq;
} p7_sender_window_t;

int p7_run_sender(int sock_fd, struct sockaddr_in *dest_addr, p7_config_t *cfg);

#endif // SENDER_H
