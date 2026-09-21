#include "debug.h"
#include "header.h"
#include <arpa/inet.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static void format_timestamp(char *buffer, size_t buffer_size);
static void format_segment_type(char *buffer, size_t buffer_size,
                                const p7_header_t *header);
static void format_bytes_num(char *buffer, size_t buffer_size,
                             const p7_header_t *header);
static void format_flags(char buffer[6], const p7_header_t *header);
static const char *format_arrow(P7_SEGMENT_DIRECTION direction);
static const char *format_direction(P7_SEGMENT_DIRECTION direction);

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

void p7_log_segment(P7_SEGMENT_DIRECTION direction, const p7_header_t *header,
                    size_t payload_len, const struct sockaddr_in *addr) {
  char time_str[40];
  const char *direction_str;
  char segment_type[16];
  char bytes_num[24];
  char flags_str[6] = "----";
  const char *arrow;

  format_timestamp(time_str, sizeof(time_str));
  direction_str = format_direction(direction);
  format_segment_type(segment_type, sizeof(segment_type), header);
  format_bytes_num(bytes_num, sizeof(bytes_num), header);
  format_flags(flags_str, header);
  arrow = format_arrow(direction);

  fprintf(stderr, "[%s] %-5s %-8s %-12s size=%-5zu flags=%s  %s %s:%d\n",
          time_str, direction_str, segment_type, bytes_num, payload_len,
          flags_str, arrow, inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
}

static void format_timestamp(char *buffer, size_t buffer_size) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  struct tm *t = localtime(&ts.tv_sec);
  char time_buffer[32];
  strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", t);
  int milliseconds = (int)(ts.tv_nsec / 1000000);
  snprintf(buffer, buffer_size, "%s.%03d", time_buffer, milliseconds);
}

static void format_segment_type(char *buffer, size_t buffer_size,
                                const p7_header_t *header) {
  if (header->flags == 0) {
    snprintf(buffer, buffer_size, "%s", "DATA");
  } else if (header->flags & P7_FIN_MASK) {
    snprintf(buffer, buffer_size, "%s", "FIN");
  } else if (header->flags & P7_RST_MASK) {
    snprintf(buffer, buffer_size, "%s", "RST");
  } else if ((header->flags & P7_SYN_MASK) && (header->flags & P7_ACK_MASK)) {
    snprintf(buffer, buffer_size, "%s", "SYN-ACK");
  } else if (header->flags & P7_SYN_MASK) {
    snprintf(buffer, buffer_size, "%s", "SYN");
  } else if (header->flags & P7_ACK_MASK) {
    snprintf(buffer, buffer_size, "%s", "ACK");
  } else {
    snprintf(buffer, buffer_size, "%s", "UNKN");
  }
}

static void format_bytes_num(char *buffer, size_t buffer_size,
                             const p7_header_t *header) {
  if (header->flags & P7_ACK_MASK) {
    snprintf(buffer, buffer_size, "ack=%" PRIu32, header->ack_num);
  } else {
    snprintf(buffer, buffer_size, "seq=%" PRIu32, header->seq_num);
  }
}

static void format_flags(char buffer[6], const p7_header_t *header) {
  if (header->flags & P7_ACK_MASK) {
    buffer[0] = 'A';
  }
  if (header->flags & P7_SYN_MASK) {
    buffer[1] = 'S';
  }
  if (header->flags & P7_FIN_MASK) {
    buffer[2] = 'F';
  }
  if (header->flags & P7_RST_MASK) {
    buffer[3] = 'R';
  }
}

static const char *format_arrow(P7_SEGMENT_DIRECTION direction) {
  if (direction == P7_SEND_DIRECTION || direction == P7_RETRY_DIRECTION)
    return "->";
  if (direction == P7_RECV_DIRECTION || direction == P7_DROP_DIRECTION)
    return "<-";
  return "-";
}

static const char *format_direction(P7_SEGMENT_DIRECTION direction) {
  if (direction == P7_SEND_DIRECTION)
    return "SEND";
  if (direction == P7_RECV_DIRECTION)
    return "RECV";
  if (direction == P7_RETRY_DIRECTION)
    return "RETR";
  if (direction == P7_DROP_DIRECTION)
    return "DROP";
  return "-";
}
