#ifndef SENDER_H
#define SENDER_H

#include <netinet/in.h>

int p7_run_sender(int sock_fd, struct sockaddr_in *sender_addr);

#endif // SENDER_H
