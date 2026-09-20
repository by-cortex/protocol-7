import argparse
import socket
from random import random

DROP_PACKETS_PERCENT = 30


def run_mock_proxy_server(proxy_port: int, dest_port: int, mode: str) -> None:
    src_port = 0
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", proxy_port))
    print(
        f"[PROXY] Server running: 127.0.0.1:{proxy_port} - 127.0.0.1:{dest_port} with {mode} mode..."
    )

    request_count = 0

    while True:
        data, addr = sock.recvfrom(2048)
        request_count += 1

        match mode:
            case "normal":
                pass
            case "drop":
                if random() < DROP_PACKETS_PERCENT / 100:
                    print(f"[DROP] {get_log_info(request_count, data, addr)} -> X")
                    continue

        if addr[1] != dest_port:
            src_port = addr[1]
            sock.sendto(data, ("127.0.0.1", dest_port))
            print(
                f"[PROXY] {get_log_info(request_count, data, addr)} -> 127.0.0.1:{dest_port}"
            )
        else:
            sock.sendto(data, ("127.0.0.1", src_port))
            print(
                f"[PROXY] {get_log_info(request_count, data, addr)} -> 127.0.0.1:{src_port}"
            )


def get_log_info(request_count: int, data: bytes, addr: tuple) -> str:
    return f"#{request_count} ({len(data)} bytes) {addr[0]}:{addr[1]}"


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--aport", type=int, default=5656)
    parser.add_argument("-d", "--bport", type=int, default=6767)
    parser.add_argument("-m", "--mode", choices=["normal", "drop"], default="normal")
    args = parser.parse_args()
    run_mock_proxy_server(args.aport, args.bport, args.mode)
