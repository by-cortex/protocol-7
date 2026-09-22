#ifndef ARGS_H
#define ARGS_H

#include <getopt.h>

#define P7_DEFAULT_HOST "127.0.0.1"
#define P7_DEFAULT_PORT 6767

#define P7_NUM_OPTIONS ((sizeof(p7_options)) / (sizeof(p7_options[0])))

typedef struct {
  const char *long_name;
  char short_name;
  int has_arg;
  const char *help;
} p7_option_t;

typedef enum { P7_MODE_NONE, P7_MODE_SEND, P7_MODE_RECV } p7_mode_t;

typedef struct {
  p7_mode_t mode;
  const char *host;
  int port;
  int hex;
  const char *file;
} p7_config_t;

static const p7_option_t p7_options[] = {
    {"send", 's', no_argument, "run as sender"},
    {"recv", 'r', no_argument, "run as receiver"},
    {"host", 'H', required_argument, "destination host (default 127.0.0.1)"},
    {"port", 'p', required_argument, "UDP port"},
    {"hex", 'x', no_argument, "print hex dump of each packet (debug)"},
    {"file", 'f', required_argument,
     "file to send (send mode) or write to (recv mode, default: stdout)"},
    {"help", 'h', no_argument, "show this help"}};

void p7_build_getopt_tables(char optstring[], struct option long_opts[]);
void p7_print_help(const char *prog);
int p7_set_cfg(p7_config_t *cfg, int opt, char *argv[]);

#endif // ARGS_H
