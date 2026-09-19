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
#include <sys/types.h>
#include <time.h>

static int p7_send_segment(p7_session_t *session, p7_header_t *header,
                           uint8_t *previous, size_t previous_bytes_read,
                           uint32_t *seq_num, p7_config_t *cfg);
static int p7_recv_ack_segment(p7_session_t *session, uint8_t *recv_buffer,
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

    if (p7_send_segment(&session, &header, previous, previous_bytes_read,
                        &seq_num, cfg) < 0) {
      fclose(fp);
      return -1;
    }
    swap_ptr(&current, &previous);
    previous_bytes_read = bytes_read;

    if (p7_recv_ack_segment(&session, recv_buffer, sizeof(recv_buffer), cfg) <
        0) {
      fclose(fp);
      return -1;
    }
  }

  if (feof(fp)) {
    header.flags |= P7_FIN_MASK;
    if (p7_send_segment(&session, &header, previous, previous_bytes_read,
                        &seq_num, cfg) < 0) {
      fclose(fp);
      return -1;
    }
    if (p7_recv_ack_segment(&session, recv_buffer, sizeof(recv_buffer), cfg) <
        0) {
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

static int p7_recv_ack_segment(p7_session_t *session, uint8_t *recv_buffer,
                               size_t recv_buffer_len, p7_config_t *cfg) {
  struct sockaddr_in ack_from_addr;
  socklen_t ack_from_addr_len = sizeof(ack_from_addr);

  ssize_t bytes_received =
      recvfrom(session->sock_fd, recv_buffer, recv_buffer_len, 0,
               (struct sockaddr *)&ack_from_addr, &ack_from_addr_len);
  if (bytes_received < 0) {
    perror("Sender recv data failed");
    return -1;
  }
  if (cfg->hex == 1) {
    p7_print_hex_dump(recv_buffer, bytes_received);
  }

  return 0;
}

static int p7_send_segment(p7_session_t *session, p7_header_t *header,
                           uint8_t *previous, size_t previous_bytes_read,
                           uint32_t *seq_num, p7_config_t *cfg) {
  p7_serialize_header(header, previous, P7_HEADER_SIZE + P7_MSS);
  if ((sendto(session->sock_fd, previous, P7_HEADER_SIZE + previous_bytes_read,
              0, (struct sockaddr *)session->addr, session->addr_len)) < 0) {
    perror("Send segment failed");
    return -1;
  }

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
