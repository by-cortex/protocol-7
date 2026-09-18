#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include "args.h"
#include <netinet/in.h>
#include <sys/socket.h>

typedef struct {
  int sock_fd;
  struct sockaddr_in *addr;
  socklen_t addr_len;
} p7_session_t;

int p7_init_socket(struct sockaddr_in *addr, p7_config_t cfg);

#endif // SOCKET_UTILS_H
