#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include "args.h"
#include <netinet/in.h>

int p7_init_socket(struct sockaddr_in *addr, p7_config_t cfg);

#endif // SOCKET_UTILS_H
