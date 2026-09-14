#include "sender.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

int p7_run_sender(int sock_fd, struct sockaddr_in *sender_addr,
                  p7_config_t cfg) {
  uint8_t chunk[1024];
  size_t bytes_read = 0;

  FILE *fp = fopen(cfg.file, "rb");
  if (fp == NULL) {
    perror("fopen");
    return -1;
  }

  while ((bytes_read = fread(chunk, 1, sizeof(chunk), fp)) > 0) {
    if ((sendto(sock_fd, chunk, bytes_read, 0, (struct sockaddr *)sender_addr,
                sizeof(*sender_addr))) < 0) {
      perror("Send UDP datagram failed");
      return -1;
    }
  }

  if (feof(fp)) {
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
