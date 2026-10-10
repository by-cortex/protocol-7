#ifndef RECEIVER_H
#define RECEIVER_H

#include "args.h"
#include "header.h"
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>

#define P7_WINDOW_SIZE 32
#define P7_MAX_WINDOW_BYTES (P7_WINDOW_SIZE * P7_MSS)

typedef struct {
  uint8_t data[P7_MSS];
  size_t data_len;
  int is_received;
  int is_fin;
} p7_recv_slot_t;

typedef struct {
  p7_recv_slot_t slots[P7_WINDOW_SIZE];
  uint32_t expected_seq;
} p7_recv_window_t;

int p7_run_receiver(int sock_fd, struct sockaddr_in *bind_addr,
                    p7_config_t *cfg);

#endif // RECEIVER_H
