#include "args.h"
#include <stddef.h>
#include <stdio.h>

void p7_build_getopt_tables(char optstring[], struct option long_opts[]) {
  size_t oi = 0;
  for (size_t i = 0; i < P7_NUM_OPTIONS; i++) {
    const p7_option_t *o = &p7_otions[i];

    optstring[oi++] = o->short_name;
    if (o->has_arg == required_argument) {
      optstring[oi++] = ':';
    }

    long_opts[i].name = o->long_name;
    long_opts[i].has_arg = o->has_arg;
    long_opts[i].flag = NULL;
    long_opts[i].val = o->short_name;
  }
  optstring[oi] = '\0';
  long_opts[P7_NUM_OPTIONS] = (struct option){0, 0, 0, 0};
}

void p7_print_help(const char *prog) {
  fprintf(stderr, "Usage: %s [options]\n\nOptions:\n", prog);
  for (size_t i = 0; i < P7_NUM_OPTIONS; i++) {
    fprintf(stderr, "  -%c, --%-10s %s\n", p7_otions[i].short_name,
            p7_otions[i].long_name, p7_otions[i].help);
  }
}
