#include "args.h"
#include "receiver.h"
#include "sender.h"
#include "socket_utils.h"
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
