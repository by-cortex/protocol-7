#ifndef RECEIVER_H
#define RECEIVER_H

#include <netinet/in.h>

int p7_run_receiver(int sock_fd, struct sockaddr_in *receiver_addr);

#endif // RECEIVER_H
