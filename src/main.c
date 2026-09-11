#include "args.h"
#include <getopt.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

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

int p7_run_receiver(int sock_fd, struct sockaddr_in *receiver_addr) {
  char buffer[1500] = {0};
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

    printf("debug recv: %s", buffer);
  }

  return 0;
}

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

int main(int argc, char *argv[]) {
  char optstring[2 * P7_NUM_OPTIONS + 1];
  struct option long_opts[P7_NUM_OPTIONS + 1];
  p7_build_getopt_tables(optstring, long_opts);
  struct sockaddr_in addr = {0};

  p7_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.mode = P7_MODE_NONE;
  cfg.host = P7_DEFAULT_HOST;
  cfg.port = P7_DEFAULT_PORT;

  int opt;
  while ((opt = getopt_long(argc, argv, optstring, long_opts, NULL)) != -1) {
    switch (opt) {
    case 's':
      cfg.mode = P7_MODE_SEND;
      break;
    case 'r':
      cfg.mode = P7_MODE_RECV;
      break;
    case 'H':
      cfg.host = optarg;
      break;
    case 'p':
      cfg.port = atoi(optarg);
      break;
    case 'h':
      p7_print_help(argv[0]);
      return 0;
    default:
      p7_print_help(argv[0]);
      return 1;
    }
  }

  if (cfg.mode == P7_MODE_NONE) {
    p7_print_help(argv[0]);
    return 1;
  }

  int sock = p7_init_socket(&addr, cfg);
  if (sock < 0) {
    return 1;
  }

  int rc = (cfg.mode == P7_MODE_SEND) ? p7_run_sender(sock, &addr)
                                      : p7_run_receiver(sock, &addr);
  close(sock);
  return rc == 0 ? 0 : 1;
}
