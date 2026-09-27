#include "receiver.h"
#include "args.h"
#include "checksum.h"
#include "debug.h"
#include "header.h"
#include "socket_utils.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#define TIMEOUT_SEC 4

static int send_ack(p7_session_t *session, uint32_t ack_num);
static int wait_double_fin(p7_session_t *session, uint8_t *recv_buffer,
                           size_t recv_buffer_len, uint32_t ack_num,
                           p7_header_t *header);

int p7_run_receiver(int sock_fd, struct sockaddr_in *bind_addr,
                    p7_config_t *cfg) {
  uint8_t recv_buffer[P7_HEADER_SIZE + P7_MSS] = {0};
  struct sockaddr_in peer_addr = {0};
  socklen_t peer_addr_len = sizeof(peer_addr);
  socklen_t bind_addr_len = sizeof(*bind_addr);
  p7_header_t header = {0};
  p7_session_t session = {0};
  FILE *fp = NULL;
  uint32_t ack_num = 0;

  session.sock_fd = sock_fd;

  if (cfg->file != NULL) {
    fp = fopen(cfg->file, "wb");
    if (fp == NULL) {
      perror("fopen");
      return -1;
    }
  }

  if (bind(sock_fd, (struct sockaddr *)bind_addr, bind_addr_len) < 0) {
    perror("Binding receiver socket failed");
    return -1;
  }

  while (!(header.flags & P7_FIN_MASK)) {
    ssize_t bytes_received =
        recvfrom(sock_fd, recv_buffer, sizeof(recv_buffer), 0,
                 (struct sockaddr *)&peer_addr, &peer_addr_len);

    if (bytes_received < 0) {
      perror("Received data failed");
      return -1;
    }

    session.addr = &peer_addr;
    session.addr_len = peer_addr_len;

    if (p7_deserialize_header(&header, recv_buffer, bytes_received) < 0 ||
        (size_t)bytes_received < P7_HEADER_SIZE) {
      continue;
    }

    uint16_t computed = p7_calculate_checksum(recv_buffer, bytes_received);
    if (computed != 0) {
      p7_log_segment(P7_DROP_DIRECTION, &header, bytes_received, &peer_addr);
      memset(&header, 0, sizeof(header));
      continue;
    }

    p7_log_segment(P7_RECV_DIRECTION, &header, bytes_received, &peer_addr);

    if (cfg->hex == 1) {
      p7_print_hex_dump(recv_buffer, bytes_received);
    }

    if (header.seq_num == ack_num) {
      ack_num += bytes_received - P7_HEADER_SIZE;

      if (cfg->file != NULL) {
        fwrite(recv_buffer + P7_HEADER_SIZE, 1, bytes_received - P7_HEADER_SIZE,
               fp);
        memset(recv_buffer, 0, sizeof(recv_buffer));
      }
    }

    if ((send_ack(&session, ack_num)) < 0) {
      return -1;
    }
  }
  if (cfg->file != NULL) {
    fclose(fp);
  }

  if ((wait_double_fin(&session, recv_buffer, sizeof(recv_buffer), ack_num,
                       &header)) < 0) {
    return -1;
  }

  printf("[Receiver] Success file transfer!\n");
  return 0;
}

static int wait_double_fin(p7_session_t *session, uint8_t *recv_buffer,
                           size_t recv_buffer_len, uint32_t ack_num,
                           p7_header_t *header) {
  uint16_t computed = 0;
  struct timeval tv = {.tv_sec = TIMEOUT_SEC, .tv_usec = 0};
  setsockopt(session->sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  while (1) {
    ssize_t bytes_received =
        recvfrom(session->sock_fd, recv_buffer, recv_buffer_len, 0,
                 (struct sockaddr *)session->addr, &session->addr_len);
    if (bytes_received < 0) {
      return 0;
    }

    if (p7_deserialize_header(header, recv_buffer, bytes_received) < 0 ||
        (size_t)bytes_received < P7_HEADER_SIZE) {
      continue;
    }

    computed = p7_calculate_checksum(recv_buffer, bytes_received);
    if (computed != 0) {
      p7_log_segment(P7_DROP_DIRECTION, header, bytes_received, session->addr);
    } else {
      p7_log_segment(P7_RECV_DIRECTION, header, bytes_received, session->addr);

      if ((send_ack(session, ack_num)) < 0) {
        return -1;
      }
      continue;
    }

    return 0;
  }
}

static int send_ack(p7_session_t *session, uint32_t ack_num) {
  uint8_t send_buffer[P7_HEADER_SIZE] = {0};
  ssize_t bytes_sent = 0;

  p7_header_t header = {0};
  header.flags |= P7_ACK_MASK;
  header.ack_num = ack_num;
  p7_serialize_header(&header, send_buffer, sizeof(send_buffer));
  header.checksum = p7_calculate_checksum(send_buffer, sizeof(send_buffer));
  p7_serialize_header(&header, send_buffer, sizeof(send_buffer));

  bytes_sent = sendto(session->sock_fd, send_buffer, sizeof(send_buffer), 0,
                      (struct sockaddr *)session->addr, session->addr_len);
  if (bytes_sent < 0) {
    perror("Send ACK failed");
    return -1;
  }

  p7_log_segment(P7_SEND_DIRECTION, &header, bytes_sent, session->addr);

  return bytes_sent;
}
