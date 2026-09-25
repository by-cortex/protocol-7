```text
██████╗ ██████╗  ██████╗ ████████╗ ██████╗  ██████╗ ██████╗ ██╗   ███████╗
██╔══██╗██╔══██╗██╔═══██╗╚══██╔══╝██╔═══██╗██╔════╝██╔═══██╗██║   ╚════██║
██████╔╝██████╔╝██║   ██║   ██║   ██║   ██║██║     ██║   ██║██║█████╗ ██╔╝
██╔═══╝ ██╔══██╗██║   ██║   ██║   ██║   ██║██║     ██║   ██║██║╚════╝██╔╝ 
██║     ██║  ██║╚██████╔╝   ██║   ╚██████╔╝╚██████╗╚██████╔╝███████╗ ██║  
╚═╝     ╚═╝  ╚═╝ ╚═════╝    ╚═╝    ╚═════╝  ╚═════╝ ╚═════╝ ╚══════╝ ╚═╝                                                                            
```

*A reliable, byte-stream transport protocol built from scratch on top of UDP — in plain C11, no dependencies beyond POSIX sockets.*

![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)
![Language](https://img.shields.io/badge/language-C11-00599C.svg)
![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)

Protocol-7 (P7) is a hand-rolled transport protocol: a custom binary wire format, checksummed segments, ARQ-based reliability, and connection teardown semantics, all implemented directly over `SOCK_DGRAM` sockets. There is no TCP stack involved beyond UDP itself — sequencing, acknowledgment, retransmission, deduplication, and closing a connection cleanly are all reimplemented here.

This is a working prototype, not a production transport. It's built to make the reliability guarantees TCP gives you for free — and the edge cases that make them hard — visible and testable. A longer write-up covering the design decisions and what broke along the way is in progress.

## Table of Contents

- [Status](#status)
- [Wire Format](#wire-format)
- [How It Works](#how-it-works)
- [Getting Started](#getting-started)
- [CLI Reference](#cli-reference)
- [Chaos Testing](#chaos-testing)
- [Verification](#verification)
- [Project Layout](#project-layout)
- [Roadmap](#roadmap)
- [Known Limitations](#known-limitations)
- [License](#license)

## Status

### Protocol / wire format

- [x] Custom binary wire format over UDP — 12-byte header: `seq_num`, `ack_num`, `checksum`, `flags`
- [x] **Byte-offset** sequencing (not packet-based) — sequence numbers track stream position, so retransmission and re-fragmentation don't desynchronize the stream
- [x] Own checksum, computed with the one's-complement algorithm used by IP/UDP/TCP — doesn't rely on the (optional, often disabled) UDP checksum
- [x] Reliable delivery via stop-and-wait ARQ: timeout + retransmission (`SO_RCVTIMEO`, bounded retry count)
- [x] Strict `ack_num` verification — an ACK is only accepted if it matches the exact expected byte offset, not "any ACK counts"
- [x] Receiver-side deduplication by `seq_num`
- [x] FIN piggybacked on the final data segment (no separate empty close packet) — implemented via a two-buffer lookahead so the sender knows a chunk is last *before* it sends it
- [x] TIME_WAIT-style teardown (`wait_double_fin`) — after sending the final ACK, the receiver keeps answering retransmitted FINs instead of exiting immediately, in case that final ACK was lost

### Infrastructure / testing

- [x] Python chaos-testing proxy (`tests/proxy.py`): `normal` / `drop` / `corrupt` / `double` relay modes, bidirectional (relays data one way and ACKs the other)
- [x] Structured, colorized segment logging — ms-precision timestamps, segment type, direction, flags, drop reason
- [x] Two build targets via Makefile: plain build and an ASan+UBSan build
- [x] Clean under `valgrind --leak-check=full --track-origins=yes`: 0 leaks, 0 errors, on both sender and receiver
- [x] CLI with `getopt_long` option tables generated from a single source-of-truth array (`p7_options[]`) — no duplicated flag definitions between parsing and `--help`

### Verified behavior

- [x] End-to-end transfer of text and binary payloads (tested with `.jpg` files), confirmed byte-for-byte with `diff` / `sha256sum`
- [x] Correct recovery under up to 30% packet loss, duplicated segments, and corrupted payloads (caught by checksum) injected via the proxy

## Wire Format

Every segment starts with a fixed 12-byte header, sent in network byte order, immediately followed by up to `P7_MSS` bytes of payload:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Sequence Number (32)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Acknowledgment Number (32)                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Checksum (16)        |    Flags (8)  | Reserved (8)  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
:                            Payload                            :
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field                     | Size    | Notes                                                                 |
|---------------------------|---------|------------------------------------------------------------------------|
| Sequence Number           | 32 bits | Byte offset of this segment's first payload byte in the stream        |
| Acknowledgment Number     | 32 bits | Next byte offset the sender/receiver expects                          |
| Checksum                  | 16 bits | One's-complement checksum over header + payload, zeroed before compute |
| Flags                     | 8 bits  | `SYN` (0x01) · `ACK` (0x02) · `FIN` (0x04) · `RST` (0x08) — top 4 bits reserved |
| Reserved                  | 8 bits  | Zeroed, reserved for future use                                       |

`SYN` and `RST` are already defined in the flags mask and understood by the logger, but there's no handshake or reset path wired up yet — see [Roadmap](#roadmap).

The **maximum segment size** is derived the same way TCP's is:

```
P7_MSS = P7_MTU (1500) − UDP header (8) − IPv4 header (20) − P7 header (12) = 1460 bytes
```

## How It Works

**No handshake, yet.** The receiver starts counting from whatever `seq_num` the first datagram it accepts carries; there's no SYN/SYN-ACK/ACK exchange establishing an initial sequence number. See [Roadmap](#roadmap).

**Sender** reads the file into one of two alternating buffers and always sends the *previous* chunk, holding the current one back — this one-chunk lookahead is what lets it know, once `fread` hits EOF, that the segment it's about to send is the last one, so it can set the `FIN` flag on that same segment instead of sending an empty FIN packet afterward. Each segment is sent with a bounded number of retries (5) against a fixed timeout (1s via `SO_RCVTIMEO`); an ACK is only accepted if `ack_num` exactly equals the byte offset just sent — a stray or delayed ACK for a different offset is ignored, not treated as confirmation.

**Receiver** validates the checksum on every incoming datagram (a mismatch is logged and dropped silently) and de-duplicates on `seq_num`: a segment is only appended to the output file if its sequence number matches the receiver's current expected offset, but an ACK for the current offset is sent either way — which is what lets the sender's retransmission recover a lost ACK without the receiver double-writing data.

**Teardown** is closer to TCP's `TIME_WAIT` than a clean handshake: once the receiver sees the `FIN` flag, it writes the last chunk, sends the final cumulative ACK, and then — instead of exiting — sits in `wait_double_fin` for a few seconds. If that final ACK was itself lost, the sender's stop-and-wait logic will retransmit the FIN segment, and the receiver re-sends the ACK instead of leaving the sender stuck retrying against silence.

```
Sender                                          Receiver
  |                                                 |
  |------ DATA  seq=0     len=1460  --------------> |
  |<----------------- ACK  ack=1460 ----------------|
  |------ DATA  seq=1460  len=1460  --------------> |
  |<----------------- ACK  ack=2920 ----------------|
  |                        ...                      |
  |------ DATA+FIN  seq=N  len=k    --------------> |   last chunk — FIN piggybacked
  |<----------------- ACK  ack=N+k -----------------|   final ACK
  |                                                 |   receiver enters wait_double_fin
  |  (final ACK lost — sender's timeout fires)      |
  |------ DATA+FIN  seq=N  len=k    --------------> |   retransmit
  |<----------------- ACK  ack=N+k -----------------|   receiver re-sends final ACK
  |                                                 |   timeout with no further FIN → closed
```

## Getting Started

### Requirements

- Linux (POSIX sockets, `SO_RCVTIMEO`)
- GCC or Clang, `make`
- Python 3 (only needed for the chaos-testing proxy in `tests/`)
- `valgrind` (optional, for the memory-check targets)

### Build

```bash
make            # plain build -> ./protocol7
make sanitize   # ASan + UBSan build -> ./protocol7-sanitized
```

### Run a transfer

Terminal 1 — receiver:

```bash
./protocol7 --recv --port 6767 --file received.bin
```

Terminal 2 — sender:

```bash
./protocol7 --send --port 6767 --file some_input.bin
```

The Makefile also exposes shortcuts for a quick local round-trip (`make recv`, `make send`, using `test.txt`) and `valgrind`-wrapped runs (`make valgrind-recv`, `make valgrind-send`).

## CLI Reference

Both sender and receiver share the same binary and option set; mode is selected with `--send`/`--recv`.

| Short | Long       | Argument | Description                                                       |
|-------|------------|----------|---------------------------------------------------------------------|
| `-s`  | `--send`   | —        | Run as sender                                                      |
| `-r`  | `--recv`   | —        | Run as receiver                                                    |
| `-H`  | `--host`   | `HOST`   | Destination host (default: `127.0.0.1`)                           |
| `-p`  | `--port`   | `PORT`   | UDP port                                                           |
| `-x`  | `--hex`    | —        | Print a hex dump of every segment (debug)                          |
| `-f`  | `--file`   | `FILE`   | File to send (send mode) or write to (receive mode)                |
| `-h`  | `--help`   | —        | Show usage                                                         |

## Chaos Testing

`tests/proxy.py` sits between sender and receiver as a UDP relay and can selectively misbehave, so reliability logic can be exercised against a channel that actually drops, duplicates, and corrupts traffic instead of trusting `localhost` to behave.

```bash
python3 tests/proxy.py --aport 5656 --bport 6767 --mode drop
```

| Mode      | Behavior                                                                 |
|-----------|---------------------------------------------------------------------------|
| `normal`  | Pure relay, no interference                                              |
| `drop`    | Drops ~30% of datagrams in transit                                       |
| `corrupt` | Flips a random byte in ~30% of datagrams before forwarding (should be caught by the checksum) |
| `double`  | Duplicates ~30% of datagrams, delivering each twice                     |

Point the receiver at `--port 6767` and the sender at `--port 5656 --host 127.0.0.1` (the proxy's listening port) so traffic flows `sender → proxy → receiver`, with ACKs relayed back the other way.

## Verification

- **Sanitizers**: the `sanitize` Makefile target builds both binaries with `-fsanitize=address,undefined` for the same test runs.
- **Memory safety**: both sender and receiver run clean under `valgrind --leak-check=full --track-origins=yes` — 0 leaks, 0 errors.
- **Data integrity**: transfers of both text and binary (`.jpg`) files are checked byte-for-byte with `diff` and `sha256sum` after each run, including under the `drop` / `corrupt` / `double` proxy modes.

## Project Layout

```
protocol-7/
├── include/            # public headers
│   ├── header.h        # wire-format struct + (de)serialization
│   ├── checksum.h       # one's-complement checksum
│   ├── args.h           # CLI option table + config struct
│   ├── socket_utils.h   # UDP socket setup, session struct
│   ├── sender.h
│   ├── receiver.h
│   └── debug.h          # segment logging / hex dump
├── src/                # implementation, mirrors include/
├── tests/
│   └── proxy.py         # chaos-testing UDP relay
├── Makefile             # plain + ASan/UBSan builds, valgrind targets
└── LICENSE
```

## Roadmap

### Toward MVP

- [ ] Sliding window — replace the current strict stop-and-wait with Go-Back-N or Selective Repeat
- [ ] 3-way handshake (`SYN` / `SYN-ACK` / `ACK`) — explicit connection establishment instead of accepting the first datagram's sequence number as-is

### Backlog / stretch (post-MVP)

- [ ] Multi-client support via hash-table demux — one socket, many concurrent sessions
- [ ] Congestion control (slow start / AIMD)
- [ ] Full duplex
- [ ] `send()`/`recv()` library API, so applications can sit on top of the protocol rather than only via the CLI
- [ ] CRC32 as an alternative/addition to the current checksum
- [ ] A demonstration of resistance (or lack thereof) to head-of-line blocking compared to TCP
- [ ] Benchmarks against TCP Reno in explicit, honestly-described scenarios (lossy channel, header overhead)

## Known Limitations

Worth being upfront about, since this is a prototype:

- Single segment in flight at a time — no window, so throughput is bounded by RTT
- No handshake — a receiver process starts trusting the first sequence number it sees
- One client per receiver process — no session demultiplexing yet
- IPv4 only
- No encryption or authentication of any kind

## License

MIT — see [LICENSE](LICENSE).
