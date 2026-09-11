#include "socket_utils.h"
#include <arpa/inet.h>
#include <stdio.h>

int p7_init_socket(struct sockaddr_in *addr, p7_config_t cfg) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("Socket creation failed");
    return -1;
  }

  addr->sin_family = AF_INET;
  addr->sin_port = htons(cfg.port);
  addr->sin_addr.s_addr = inet_addr(cfg.host);

  return sock;
}
