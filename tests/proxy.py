import argparse
import socket
from random import randint, random


def corrupt_data(data: bytes) -> bytes:
    if not data:
        return data
    mutable_data = bytearray(data)
    idx = randint(0, len(data) - 1)
    original = data[idx]
    new_byte = randint(0, 255)
    while new_byte == original:
        new_byte = randint(0, 255)
    mutable_data[idx] = new_byte
    return bytes(mutable_data)


def run_mock_proxy_server(
    proxy_port: int,
    dest_port: int,
    mode: str,
    drop_pct: int,
    corrupt_pct: int,
    double_pct: int,
) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", proxy_port))

    print(
        f"[PROXY] Running on 127.0.0.1:{proxy_port} -> forwarding to 127.0.0.1:{dest_port}"
    )
    print(
        f"[PROXY] Mode: {mode.upper()} | Drop: {drop_pct}% | Corrupt: {corrupt_pct}% | Double: {double_pct}%"
    )
    print("-" * 70)

    client_addr = None
    request_count = 0

    while True:
        try:
            data, addr = sock.recvfrom(65535)
        except Exception as e:
            print(f"[PROXY ERROR] recvfrom failed: {e}")
            break

        request_count += 1

        is_from_client = addr[1] != dest_port

        if is_from_client:
            client_addr = addr
            target_addr = ("127.0.0.1", dest_port)
            direction_str = "CLIENT -> SERVER"
        else:
            if client_addr is None:
                continue
            target_addr = client_addr
            direction_str = "SERVER -> CLIENT"

        packets_to_send = [data]

        match mode:
            case "normal":
                pass

            case "drop":
                if random() < (drop_pct / 100.0):
                    print(
                        f"[DROP] #{request_count} ({len(data)} bytes) [{direction_str}] X-> Dropped"
                    )
                    continue

            case "corrupt":
                if random() < (corrupt_pct / 100.0):
                    packets_to_send = [corrupt_data(data)]
                    print(
                        f"[CORRUPT] #{request_count} ({len(data)} bytes) [{direction_str}] -> Byte flipped!"
                    )

            case "double":
                if random() < (double_pct / 100.0):
                    packets_to_send = [data, data]
                    print(
                        f"[DOUBLE] #{request_count} ({len(data)} bytes) [{direction_str}] -> Duplicated!"
                    )

        for pkt in packets_to_send:
            try:
                sock.sendto(pkt, target_addr)
            except Exception as e:
                print(f"[PROXY ERROR] sendto failed: {e}")

        print(
            f"[FWD]   #{request_count} ({len(data)} bytes) [{direction_str}] -> 127.0.0.1:{target_addr[1]}"
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="P7 Protocol Network Impairment Proxy")
    parser.add_argument(
        "-p", "--aport", type=int, default=5656, help="Proxy listen port"
    )
    parser.add_argument(
        "-d", "--bport", type=int, default=6767, help="Destination server port"
    )
    parser.add_argument(
        "-m",
        "--mode",
        choices=["normal", "drop", "corrupt", "double"],
        default="normal",
        help="Network impairment mode",
    )
    parser.add_argument(
        "--drop-rate", type=int, default=30, help="Drop probability percentage"
    )
    parser.add_argument(
        "--corrupt-rate", type=int, default=30, help="Corruption probability percentage"
    )
    parser.add_argument(
        "--double-rate", type=int, default=30, help="Duplication probability percentage"
    )

    args = parser.parse_args()

    try:
        run_mock_proxy_server(
            proxy_port=args.aport,
            dest_port=args.bport,
            mode=args.mode,
            drop_pct=args.drop_rate,
            corrupt_pct=args.corrupt_rate,
            double_pct=args.double_rate,
        )
    except KeyboardInterrupt:
        print("\n[PROXY] Stopped by user.")
