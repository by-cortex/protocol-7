#include "debug.h"
#include "header.h"

void p7_print_hex_dump(const uint8_t *buffer, ssize_t buffer_len) {
  fprintf(stderr, "--- HEADER ---\n");
  for (int i = 0; i < P7_HEADER_SIZE; i++) {
    fprintf(stderr, "%02x ", buffer[i]);
    if ((i + 1) % 12 == 0 && i != 0)
      fprintf(stderr, "\n");
  }

  fprintf(stderr, "\n\n--- PAYLOAD ---\n");
  for (int i = P7_HEADER_SIZE; i < (int)buffer_len; i++) {
    fprintf(stderr, "%02x ", buffer[i]);
    if ((i + 1) % 12 == 0 && i != 0)
      fprintf(stderr, "\n");
  }
  fprintf(stderr, "\n\n");
}
