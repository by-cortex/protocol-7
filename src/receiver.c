#include "receiver.h"
#include "args.h"
#include "debug.h"
#include "header.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

static int send_ack(int sock_fd, int ack_num, struct sockaddr *addr,
                    socklen_t addr_len);

int p7_run_receiver(int sock_fd, struct sockaddr_in *receiver_addr,
                    p7_config_t *cfg) {
  uint8_t buffer[P7_HEADER_SIZE + P7_MSS] = {0};
  socklen_t receiver_addr_len = sizeof(*receiver_addr);
  struct sockaddr_in peer_addr = {0};
  socklen_t peer_addr_len = sizeof(peer_addr);
  p7_header_t header = {0};
  FILE *fp = NULL;

  if (cfg->file != NULL) {
    fp = fopen(cfg->file, "wb");
    if (fp == NULL) {
      perror("fopen");
      return -1;
    }
  }

  if (bind(sock_fd, (struct sockaddr *)receiver_addr, receiver_addr_len) < 0) {
    perror("Binding receiver socket failed");
    return -1;
  }

  while (!(header.flags & P7_FIN_MASK)) {
    ssize_t bytes_received =
        recvfrom(sock_fd, buffer, sizeof(buffer), 0,
                 (struct sockaddr *)&peer_addr, &peer_addr_len);

    if (bytes_received < 0) {
      perror("Received from failed");
      return -1;
    }
    p7_deserialize_header(&header, buffer, bytes_received);

    if ((send_ack(sock_fd, header.seq_num + (bytes_received - P7_HEADER_SIZE),
                  (struct sockaddr *)&peer_addr, peer_addr_len)) < 0) {
      return -1;
    }

    if (cfg->hex == 1) {
      p7_print_hex_dump(buffer, bytes_received);
    }
    if (cfg->file != NULL) {
      fwrite(buffer + P7_HEADER_SIZE, 1, bytes_received - P7_HEADER_SIZE, fp);
      memset(buffer, 0, sizeof(buffer));
    }
  }
  if (cfg->file != NULL) {
    fclose(fp);
  }
  printf("[Receiver] Success file transfer!\n");

  return 0;
}

static int send_ack(int sock_fd, int ack_num, struct sockaddr *addr,
                    socklen_t addr_len) {
  uint8_t buffer[P7_HEADER_SIZE] = {0};
  p7_header_t header = {0};
  int bytes_sent = 0;
  header.flags |= P7_ACK_MASK;
  header.ack_num = ack_num;

  p7_serialize_header(&header, buffer, sizeof(buffer));
  if ((bytes_sent =
           sendto(sock_fd, buffer, sizeof(buffer), 0, addr, addr_len) < 0)) {
    perror("Send ACK segment failed");
    return -1;
  }
  return bytes_sent;
}
