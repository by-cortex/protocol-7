#include "sender.h"
#include "args.h"
#include "checksum.h"
#include "debug.h"
#include "header.h"
#include "socket_utils.h"
#include <asm-generic/errno-base.h>
#include <bits/time.h>
#include <bits/types/struct_timeval.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

static uint64_t get_time_ms(void);

int p7_run_sender(int sock_fd, struct sockaddr_in *dest_addr,
                  p7_config_t *cfg) {

  p7_session_t session = {
      .sock_fd = sock_fd,
      .addr = dest_addr,
      .addr_len = sizeof(*dest_addr),
  };

  FILE *fp = fopen(cfg->file, "rb");
  if (fp == NULL) {
    perror("fopen");
    return -1;
  }

  p7_sender_window_t *window = calloc(1, sizeof(p7_sender_window_t));
  if (!window) {
    fclose(fp);
    return -1;
  }

  int is_eof = 0;

  struct pollfd pfd = {
      .fd = session.sock_fd,
      .events = POLLIN,
      .revents = 0,
  };
  int timeout_ms = 50;

  while (1) {
    if (is_eof && window->base == window->new_seq) {
      printf("[Sender] Transfer complete successfully!\n");
      break;
    }

    uint64_t now_ms = get_time_ms();

    // -------------------------------
    // Window filling
    // -------------------------------

    while (!is_eof) {
      uint32_t in_flight_bytes = window->new_seq - window->base;
      uint32_t max_window_bytes = P7_WINDOW_SIZE * P7_MSS;

      if (in_flight_bytes >= max_window_bytes) {
        break;
      }

      uint32_t segment_idx = window->new_seq / P7_MSS;
      uint32_t slot_idx = segment_idx % P7_WINDOW_SIZE;

      p7_sender_slot_t *slot = &window->slots[slot_idx];

      size_t bytes_read = fread(slot->data + P7_HEADER_SIZE, 1, P7_MSS, fp);
      if (bytes_read == 0) {
        if (feof(fp)) {
          is_eof = 1;
        }
        break;
      }

      slot->seq_num = window->new_seq;
      slot->data_len = P7_HEADER_SIZE + bytes_read;
      slot->is_acked = 0;
      slot->send_time_ms = now_ms;

      p7_header_t header = {0};
      header.seq_num = slot->seq_num;
      if (feof(fp)) {
        header.flags |= P7_FIN_MASK;
      }

      p7_serialize_header(&header, slot->data, P7_HEADER_SIZE);
      header.checksum = p7_calculate_checksum(slot->data, slot->data_len);
      p7_serialize_header(&header, slot->data, P7_HEADER_SIZE);

      ssize_t bytes_send =
          sendto(session.sock_fd, slot->data, slot->data_len, 0,
                 (struct sockaddr *)session.addr, session.addr_len);
      if (bytes_send < 0) {
        perror("Failed send segment");
      }
      p7_log_segment(P7_SEND_DIRECTION, &header, slot->data_len, session.addr);
      window->new_seq += bytes_read;
    }

    // -------------------------------

    int activity = poll(&pfd, 1, timeout_ms);

    if (activity < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("Poll error");
      break;
    }

    // -------------------------------
    // Recv ACK/SACK
    // -------------------------------

    if (activity > 0) {
      if (pfd.revents & POLLIN) {
        uint8_t ack_buffer[P7_HEADER_SIZE + P7_MSS] = {0};
        struct sockaddr_in ack_addr = {0};
        socklen_t ack_addr_len = sizeof(ack_addr);

        ssize_t bytes_recv =
            recvfrom(session.sock_fd, ack_buffer, sizeof(ack_buffer), 0,
                     (struct sockaddr *)&ack_addr, &ack_addr_len);
        if (bytes_recv > 0 &&
            p7_calculate_checksum(ack_buffer, bytes_recv) == 0) {
          p7_header_t ack_header = {0};
          p7_deserialize_header(&ack_header, ack_buffer, sizeof(ack_buffer));

          p7_log_segment(P7_RECV_DIRECTION, &ack_header, bytes_recv, &ack_addr);

          if (ack_header.ack_num > window->base) {
            window->base = ack_header.ack_num;
          }

          // TODO: SACK

          while (window->base < window->new_seq) {
            uint32_t base_idx = (window->base / P7_MSS) % P7_WINDOW_SIZE;
            if (window->slots[base_idx].is_acked) {
              window->base +=
                  (window->slots[base_idx].data_len - P7_HEADER_SIZE);
            } else {
              break;
            }
          }
        }
      }
    }

    // -------------------------------
    // Selective Repeat
    // -------------------------------

    if (activity == 0) {
      now_ms = get_time_ms();
      uint32_t start_seg_idx = (window->base / P7_MSS) % P7_WINDOW_SIZE;
      uint32_t end_seg_idx = (window->new_seq / P7_MSS) % P7_WINDOW_SIZE;

      for (uint32_t i = start_seg_idx; i <= end_seg_idx; i++) {
        p7_sender_slot_t *slot = &window->slots[i];

        if (!slot->is_acked && (now_ms - slot->send_time_ms > P7_TIMEOUT_MS)) {
          sendto(session.sock_fd, slot->data, slot->data_len, 0,
                 (struct sockaddr *)dest_addr, sizeof(*dest_addr));

          p7_header_t header = {0};
          p7_deserialize_header(&header, slot->data, slot->data_len);
          p7_log_segment(P7_SEND_DIRECTION, &header, slot->data_len,
                         session.addr);

          slot->send_time_ms = now_ms;
        }
      }
    }
  }

  free(window);
  fclose(fp);
  return 0;
}

static uint64_t get_time_ms(void) {
  struct timespec ts = {0};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
