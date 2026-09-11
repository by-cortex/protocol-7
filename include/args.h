#ifndef ARGS_H
#define ARGS_H

#include <getopt.h>

#define P7_NUM_OPTIONS ((sizeof(p7_otions)) / (sizeof(p7_otions[0])))

typedef struct {
  const char *long_name;
  char short_name;
  int has_arg;
  const char *help;
} p7_option_t;

static const p7_option_t p7_otions[] = {
    {"host", 'H', required_argument, "host flag"},
    {"port", 'p', required_argument, "port flag"},
    {"help", 'h', no_argument, "this message"}};

void p7_build_getopt_tables(char optstring[], struct option long_opts[]);
void p7_print_help(const char *prog);

#endif // ARGS_H
