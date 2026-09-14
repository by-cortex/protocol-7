#ifndef SENDER_H
#define SENDER_H

#include "args.h"
#include <netinet/in.h>

int p7_run_sender(int sock_fd, struct sockaddr_in *sender_addr,
                  p7_config_t cfg);

#endif // SENDER_H
