#include "sender.h"
#include "args.h"
#include "debug.h"
#include "header.h"
#include "socket_utils.h"
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#define MAX_RETRIES 5
#define TIMEOUT_SEC 1

static int send_with_retry(p7_session_t *session, p7_header_t *header,
                           uint8_t *previous, size_t previous_bytes_read,
                           uint32_t *seq_num, p7_config_t *cfg,
                           uint8_t *recv_buffer, size_t recv_buffer_len);
static int send_segment(p7_session_t *session, p7_header_t *header,
                        uint8_t *previous, size_t previous_bytes_read,
                        uint32_t *seq_num, p7_config_t *cfg);
static int recv_ack_segment(p7_session_t *session, uint8_t *recv_buffer,
                            size_t recv_buffer_len, p7_config_t *cfg);
static void swap_ptr(uint8_t **buff_a, uint8_t **buff_b);

int p7_run_sender(int sock_fd, struct sockaddr_in *dest_addr,
                  p7_config_t *cfg) {
  uint8_t buffer_a[P7_HEADER_SIZE + P7_MSS] = {0};
  uint8_t buffer_b[P7_HEADER_SIZE + P7_MSS] = {0};
  uint8_t *current = buffer_a;
  uint8_t *previous = buffer_b;
  size_t bytes_read = 0;
  size_t previous_bytes_read = 0;
  int is_previous = 0;
  p7_header_t header = {0};
  p7_session_t session = {0};
  uint32_t seq_num = 0;
  uint8_t recv_buffer[P7_HEADER_SIZE + P7_MSS] = {0};

  session.sock_fd = sock_fd;
  session.addr = dest_addr;
  session.addr_len = sizeof(*dest_addr);

  FILE *fp = fopen(cfg->file, "rb");
  if (fp == NULL) {
    perror("fopen");
    return -1;
  }

  while ((bytes_read = fread(current + P7_HEADER_SIZE, 1, P7_MSS, fp)) > 0) {
    if (is_previous == 0) {
      swap_ptr(&current, &previous);
      previous_bytes_read = bytes_read;
      is_previous = 1;
      continue;
    }

    ssize_t bytes_sent =
        send_with_retry(&session, &header, previous, previous_bytes_read,
                        &seq_num, cfg, recv_buffer, sizeof(recv_buffer));
    if (bytes_sent < 0) {
      fclose(fp);
      return -1;
    }

    swap_ptr(&current, &previous);
    previous_bytes_read = bytes_read;
  }

  if (feof(fp)) {
    header.flags |= P7_FIN_MASK;
    ssize_t bytes_sent =
        send_with_retry(&session, &header, previous, previous_bytes_read,
                        &seq_num, cfg, recv_buffer, sizeof(recv_buffer));
    if (bytes_sent < 0) {
      fclose(fp);
      return -1;
    }

    fclose(fp);
    printf("[Sender] Success file transfer!\n");
    return 0;
  } else if (ferror(fp)) {
    perror("fread");
    fclose(fp);
    return -1;
  }
  return 0;
}

static int send_with_retry(p7_session_t *session, p7_header_t *header,
                           uint8_t *previous, size_t previous_bytes_read,
                           uint32_t *seq_num, p7_config_t *cfg,
                           uint8_t *recv_buffer, size_t recv_buffer_len) {
  int attempts = 0;
  while (attempts <= MAX_RETRIES) {
    if (send_segment(session, header, previous, previous_bytes_read, seq_num,
                     cfg) < 0) {
      return -1;
    }

    struct timeval tv = {.tv_sec = TIMEOUT_SEC, .tv_usec = 0};
    setsockopt(session->sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    ssize_t n = recv_ack_segment(session, recv_buffer, recv_buffer_len, cfg);
    if (n > 0) {
      return n;
    }

    attempts++;
  }
  fprintf(stderr, "[Sender] Receive ACK segment attempts limit\n");
  return -1;
}

static int recv_ack_segment(p7_session_t *session, uint8_t *recv_buffer,
                            size_t recv_buffer_len, p7_config_t *cfg) {
  struct sockaddr_in ack_from_addr;
  socklen_t ack_from_addr_len = sizeof(ack_from_addr);
  p7_header_t header = {0};

  ssize_t bytes_received =
      recvfrom(session->sock_fd, recv_buffer, recv_buffer_len, 0,
               (struct sockaddr *)&ack_from_addr, &ack_from_addr_len);
  if (bytes_received < 0) {
    return -1;
  }

  p7_deserialize_header(&header, recv_buffer, recv_buffer_len);

  p7_log_segment(P7_RECV_DIRECTION, &header, bytes_received, &ack_from_addr);

  if (cfg->hex == 1) {
    p7_print_hex_dump(recv_buffer, bytes_received);
  }
  return bytes_received;
}

static int send_segment(p7_session_t *session, p7_header_t *header,
                        uint8_t *previous, size_t previous_bytes_read,
                        uint32_t *seq_num, p7_config_t *cfg) {
  ssize_t bytes_sent = 0;
  p7_serialize_header(header, previous, P7_HEADER_SIZE + P7_MSS);
  bytes_sent =
      sendto(session->sock_fd, previous, P7_HEADER_SIZE + previous_bytes_read,
             0, (struct sockaddr *)session->addr, session->addr_len);
  if (bytes_sent < 0) {
    perror("Send segment failed");
    return -1;
  }

  p7_log_segment(P7_SEND_DIRECTION, header, bytes_sent, session->addr);

  *seq_num += previous_bytes_read;
  header->seq_num = *seq_num;

  if (cfg->hex == 1) {
    p7_print_hex_dump(previous, previous_bytes_read);
  }
  return 0;
}

static void swap_ptr(uint8_t **buff_a, uint8_t **buff_b) {
  uint8_t *tmp = *buff_a;
  *buff_a = *buff_b;
  *buff_b = tmp;
}
