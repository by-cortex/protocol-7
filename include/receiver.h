#ifndef RECEIVER_H
#define RECEIVER_H

#include "args.h"
#include <netinet/in.h>

int p7_run_receiver(int sock_fd, struct sockaddr_in *bind_addr,
                    p7_config_t *cfg);

#endif // RECEIVER_H
