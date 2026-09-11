#include "args.h"
#include <getopt.h>
#include <stddef.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  char optstring[2 * P7_NUM_OPTIONS + 1];
  struct option long_opts[P7_NUM_OPTIONS + 1];
  p7_build_getopt_tables(optstring, long_opts);

  int opt;
  while ((opt = getopt_long(argc, argv, optstring, long_opts, NULL)) != -1) {
    switch (opt) {
    case 'h':
      p7_print_help(argv[0]);
      return 0;
    default:
      p7_print_help(argv[0]);
      return 1;
    }
  }

  printf("Hello protocol!\n");
  return 0;
}
