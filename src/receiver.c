#include "receiver.h"
#include "args.h"
#include "debug.h"
#include "header.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int p7_run_receiver(int sock_fd, struct sockaddr_in *receiver_addr,
                    p7_config_t *cfg) {
  uint8_t buffer[P7_HEADER_SIZE + P7_MSS] = {0};
  socklen_t receiver_addr_len = sizeof(*receiver_addr);
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
    perror("Binding socket failed");
    return -1;
  }

  while (!(header.flags & P7_FIN_MASK)) {
    ssize_t bytes_received =
        recvfrom(sock_fd, buffer, sizeof(buffer), 0,
                 (struct sockaddr *)receiver_addr, &receiver_addr_len);

    if (bytes_received < 0) {
      perror("Received from failed");
      return -1;
    }
    p7_deserialize_header(&header, buffer, bytes_received);

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
