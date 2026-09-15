#include "receiver.h"
#include "header.h"
#include <stdint.h>
#include <stdio.h>

int p7_run_receiver(int sock_fd, struct sockaddr_in *receiver_addr) {
  uint8_t buffer[P7_HEADER_SIZE + P7_MSS] = {0};
  socklen_t receiver_addr_len = sizeof(*receiver_addr);

  if (bind(sock_fd, (struct sockaddr *)receiver_addr, receiver_addr_len) < 0) {
    perror("Binding socket failed");
    return -1;
  }

  while (1) {
    ssize_t bytes_received =
        recvfrom(sock_fd, buffer, sizeof(buffer), 0,
                 (struct sockaddr *)receiver_addr, &receiver_addr_len);

    if (bytes_received < 0) {
      perror("Received from failed");
      return -1;
    }
    // fwrite(buffer, 1, bytes_received, stdout);

    // DEBUG
    fprintf(stderr, "--- HEADER ---\n");
    for (int i = 0; i < P7_HEADER_SIZE; i++) {
      fprintf(stderr, "%02x ", buffer[i]);
      if ((i + 1) % 12 == 0 && i != 0)
        fprintf(stderr, "\n");
    }

    fprintf(stderr, "\n\n--- PAYLOAD ---\n");
    for (int i = P7_HEADER_SIZE; i < bytes_received; i++) {
      fprintf(stderr, "%02x ", buffer[i]);
      if ((i + 1) % 12 == 0 && i != 0)
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n\n");
    // ---
  }

  return 0;
}
