*A reliable, pipelined byte-stream transport protocol built from scratch on top of UDP — in plain C11, featuring a sliding window, bitmask-based SACK, and fast retransmission.*

![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)
![Language](https://img.shields.io/badge/language-C11-00599C.svg)
![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)

Protocol-7 (P7) is a hand-rolled transport protocol implemented directly over POSIX `SOCK_DGRAM` sockets. It recreates core TCP reliability mechanics from the ground up: byte-offset stream tracking, a 16-bit word-aligned checksum, pipelined sliding window transmission, selective acknowledgments (SACK), fast retransmission, and graceful connection teardown.

---

## Table of Contents

- [Features](#features)
- [Wire Format](#wire-format)
- [How It Works](#how-it-works)
- [Getting Started](#getting-started)
- [CLI Reference](#cli-reference)
- [Chaos Testing & Benchmarks](#chaos-testing--benchmarks)
- [Verification](#verification)
- [Project Layout](#project-layout)
- [Roadmap](#roadmap)
- [Known Limitations](#known-limitations)
- [License](#license)

---

## Features

### Protocol & Reliability

- **Sliding Window Pipelining:** Fixed-size 32-slot ring buffer allowing multiple unacknowledged segments in flight.
- **I/O Multiplexing via `poll()`:** Non-blocking event loop handling segment bursts, incoming ACKs, and timeout scanning concurrently.
- **Byte-Offset Sequencing:** Continuous byte-stream positioning (like TCP), independent of segment chunk sizes.
- **Selective ACK (SACK):** Custom 32-bit ACK mask header field allowing the receiver to report out-of-order buffered blocks.
- **Fast Retransmit (RFC 5681):** Triggers immediate retransmission upon receiving 3 duplicate ACKs without waiting for coarse RTO timers.
- **Selective Repeat:** Retransmits only unacknowledged (`!is_acked`) expired slots rather than resetting the entire window (Go-Back-N).
- **16-bit Word-Aligned Checksum:** One's-complement arithmetic with end-around carry, correctly positioned at even byte boundaries to guarantee `0x0000` self-cancellation.
- **Piggybacked FIN:** Look-ahead double buffering allows setting the `FIN` flag on the final payload chunk, saving a round-trip.
- **TIME_WAIT State:** Receiver maintains an active wait period after final ACK to catch and resolve lost FIN retransmissions.

### Tooling & Verification

- **Chaos Proxy:** Python UDP relay simulating packet loss, byte corruption, and duplicate delivery.
- **Automated Benchmarks:** Test suite measuring transfer throughput, integrity, and loss recovery across modes.
- **Zero Memory Leaks:** Clean runs under `valgrind --leak-check=full` and AddressSanitizer (`-fsanitize=address,undefined`).

---

## Wire Format

Every segment starts with a fixed **16-byte header** sent in network byte order (Big-Endian), followed by up to `P7_MSS` bytes of payload:

```text
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Sequence Number (32)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    Acknowledgment Number (32)                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                      Selective ACK Mask (32)                  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Checksum (16)        |    Flags (8)  | Reserved (8)  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
:                            Payload                            :
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field                  | Size    | Description                                                            |
|------------------------|---------|------------------------------------------------------------------------|
| **Sequence Number**    | 32 bits | Byte offset of the segment's first payload byte in the stream.         |
| **Acknowledgment Number** | 32 bits | Cumulative ACK: the next contiguous byte offset the receiver expects. |
| **Selective ACK Mask** | 32 bits | Bitmask tracking out-of-order received slots relative to `ack_num`.     |
| **Checksum**           | 16 bits | One's-complement sum over header + payload (aligned to byte 8).        |
| **Flags**              | 8 bits  | Control bits: `SYN` (0x01), `ACK` (0x02), `FIN` (0x04), `RST` (0x08).    |
| **Reserved**           | 8 bits  | Zeroed padding for memory structure alignment.                         |

The **Maximum Segment Size (MSS)** is derived from standard Ethernet MTU:

```text
P7_MSS = 1500 (MTU) − 20 (IPv4) − 8 (UDP) − 16 (P7 Header) = 1456 bytes
```

---

## How It Works

### Sender Pipeline
1. **Window Filling:** Computes in-flight bytes as `new_seq - base`. While `in_flight < (P7_WINDOW_SIZE * P7_MSS)`, reads file chunks and immediately transmits them via `sendto()`.
2. **Event Loop (`poll`):** Listens with a 50ms timeout.
   - **On incoming ACK (`activity > 0`):** Advances `base` cumulatively. Parses the 32-bit `ack_mask` to mark out-of-order buffered slots as acknowledged (`is_acked = 1`).
   - **Fast Retransmit:** Tracks consecutive duplicate ACKs. Upon reaching 3 dup-ACKs, immediately retransmits the missing slot without waiting for a timeout.
   - **On Timeout (`activity == 0`):** Scans active in-flight slots and retransmits only unacknowledged segments whose `send_time_ms` exceeded the RTO threshold.

### Receiver & Reassembly
1. Validates the one's-complement checksum (corrupted packets are dropped silently).
2. Segments falling within the receive window are stored in their corresponding slot (`is_received = 1`).
3. **Sequential Disk Flush:** Continuously drains consecutive received slots to disk starting from `expected_seq`. Halts upon encountering the first gap.
4. Generates a cumulative `ack_num` (pointing to the gap) combined with an `ack_mask` representing out-of-order segments buffered ahead of the gap.

### Teardown (TIME_WAIT)
- The sender sets the `FIN` flag on the final data chunk using double-buffering lookahead.
- Upon receiving `FIN` and flushing all data, the receiver transmits the final ACK and enters a `TIME_WAIT` loop. It stays active for several seconds, re-answering any duplicate FIN segments in case its final ACK was dropped.

---

## Getting Started

### Requirements
- Linux (x86_64 / arm64)
- GCC or Clang (supporting C11), `make`
- Python 3 (for chaos proxy and benchmark runner)

### Build

```bash
make            # Optimized build -> ./protocol7
make sanitize   # ASan + UBSan build -> ./protocol7-sanitized
```

### Quick Transfer

Terminal 1 — Receiver:
```bash
./protocol7 -r -p 6767 -f received.bin
```

Terminal 2 — Sender:
```bash
./protocol7 -s -H 127.0.0.1 -p 6767 -f input.bin
```

---

## CLI Reference

Both sender and receiver run from the unified binary:

| Flag | Long Flag | Parameter | Description                                              |
|------|-----------|-----------|----------------------------------------------------------|
| `-s` | `--send`  | —         | Run in sender mode.                                      |
| `-r` | `--recv`  | —         | Run in receiver mode.                                    |
| `-H` | `--host`  | `HOST`    | Destination host address (default: `127.0.0.1`).         |
| `-p` | `--port`  | `PORT`    | UDP port.                                                |
| `-f` | `--file`  | `FILE`    | Source file (send mode) or destination path (recv mode). |
| `-x` | `--hex`   | —         | Print hex dump of transmitted packets (debug).           |
| `-h` | `--help`  | —         | Show CLI help.                                           |

---

## Chaos Testing & Benchmarks

The Python proxy (`tests/proxy.py`) sits between sender and receiver to inject simulated network impairments:

```bash
python3 tests/proxy.py --aport 5656 --bport 6767 --mode drop
```

| Mode      | Impairment Simulation                                          |
|-----------|----------------------------------------------------------------|
| `normal`  | Transparent bidirectional proxying.                            |
| `drop`    | Randomly drops of datagrams.                                   |
| `corrupt` | Inverts random payload bits (triggers checksum drops).         |
| `double`  | Re-delivers datagram duplicates (tests receiver de-duplication). |

### Benchmark Results (10 MB Payload, Window = 32)

```text
=== RESULT TABLE ===
Mode       | Size (MB)  | Impairment           | Time (s)   | Speed (MB/s)    | Integrity
-------------------------------------------------------------------------------------
NORMAL     | 10         | None                 | 0.151      | 66.19           | PASS
DROP       | 10         | Drop: 5%             | 5.403      | 1.85            | PASS
DROP       | 10         | Drop: 15%            | 121.971    | 0.08            | PASS
CORRUPT    | 10         | Corrupt: 5%          | 16.683     | 0.60            | PASS
DOUBLE     | 10         | Double: 10%          | 0.197      | 50.74           | PASS
```

---

## Verification

- **Data Integrity:** Byte-for-byte fidelity confirmed via `diff` and `sha256sum` across all impairment modes.
- **Memory Cleanliness:** 0 memory leaks, 0 uninitialized byte access reported under `valgrind --leak-check=full` and ASan.

---

## Project Layout

```text
protocol-7/
├── include/            # C Header files
│   ├── header.h        # 16-byte wire-format definition & serialization
│   ├── checksum.h      # Word-aligned one's-complement checksum
│   ├── args.h          # Command-line parsing
│   ├── socket_utils.h  # POSIX UDP socket wrappers
│   ├── sender.h        # Sliding window, poll reactor, retransmission
│   ├── receiver.h      # Receive buffer queue, sequential file sink
│   └── debug.h         # Colorized timestamp logging & hex dumps
├── src/                # Implementation sources
├── tests/
│   ├── proxy.py        # Python chaos proxy (drop/corrupt/double)
│   └── benchmark.py    # Automated test runner
├── Makefile            # Targets: default, sanitize, valgrind
└── LICENSE
```

---

## Roadmap

- [ ] 3-Way Handshake (`SYN` / `SYN-ACK` / `ACK`) with randomized Initial Sequence Numbers (ISN).
- [ ] Congestion Control (Slow Start, AIMD / Reno-like dynamics).
- [ ] Jacobson/Karels dynamic RTT estimation and adaptive RTO calculation.
- [ ] Silly Window Syndrome avoidance (Nagle's and Clark's algorithms).
- [ ] Connection demultiplexing (concurrent multi-client sessions over a single socket).

---

## Known Limitations

- **Fixed Window Size:** Window size is statically set to 32 slots; lacks dynamic window scaling (`wscale`).
- **No Handshake:** Transfers begin immediately with implicit stream initialization.
- **Single Session:** The receiver accepts datagrams from one client per process lifetime.
- **IPv4 Only:** No IPv6 socket support.

---

## License

MIT — see [LICENSE](LICENSE).
