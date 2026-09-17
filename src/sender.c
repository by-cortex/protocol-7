#include "sender.h"
#include "header.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>

static void swap_ptr(uint8_t **buff_a, uint8_t **buff_b) {
  uint8_t *tmp = *buff_a;
  *buff_a = *buff_b;
  *buff_b = tmp;
}

int p7_run_sender(int sock_fd, struct sockaddr_in *sender_addr,
                  p7_config_t *cfg) {
  uint8_t buffer_a[P7_HEADER_SIZE + P7_MSS] = {0};
  uint8_t buffer_b[P7_HEADER_SIZE + P7_MSS] = {0};
  uint8_t *current = buffer_a;
  uint8_t *previous = buffer_b;
  int is_previous = 0;
  size_t bytes_read = 0;
  size_t previous_bytes_read = 0;
  p7_header_t header = {0};

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

    p7_serialize_header(&header, previous, P7_HEADER_SIZE + P7_MSS);
    if ((sendto(sock_fd, previous, P7_HEADER_SIZE + previous_bytes_read, 0,
                (struct sockaddr *)sender_addr, sizeof(*sender_addr))) < 0) {
      perror("Send UDP datagram failed");
      return -1;
    }
    swap_ptr(&current, &previous);
    previous_bytes_read = bytes_read;
  }

  if (feof(fp)) {
    header.flags |= P7_FIN_MASK;
    p7_serialize_header(&header, previous, P7_HEADER_SIZE + P7_MSS);
    if ((sendto(sock_fd, previous, P7_HEADER_SIZE + previous_bytes_read, 0,
                (struct sockaddr *)sender_addr, sizeof(*sender_addr))) < 0) {
      perror("Send UDP datagram failed");
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
