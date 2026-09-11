#include "sender.h"
#include <stdio.h>

int p7_run_sender(int sock_fd, struct sockaddr_in *sender_addr) {
  char buffer[1500] = "Hello from sender\n";

  for (int i = 0; i < 3; i++) {
    if ((sendto(sock_fd, buffer, sizeof(buffer), 0,
                (struct sockaddr *)sender_addr, sizeof(*sender_addr))) < 0) {
      perror("Send UDP datagram failed");
      return -1;
    }
    printf("debug send: %s", buffer);
  }
  return 0;
}
