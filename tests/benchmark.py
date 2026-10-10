import hashlib
import os
import subprocess
import sys
import time

BINARY = "./protocol7"
PROXY_SCRIPT = "tests/proxy.py"
TEST_FILE = "benchmark_payload.bin"
RECEIVED_FILE = "benchmark_received.bin"

PROXY_PORT = 5656
SERVER_PORT = 6767


def calculate_md5(filepath):
    hasher = hashlib.md5()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def generate_file(size_mb):
    print(f"[*] Generating {size_mb} MB test file...")
    chunk = os.urandom(1024 * 1024)
    with open(TEST_FILE, "wb") as f:
        for _ in range(size_mb):
            f.write(chunk)


def run_benchmark_scenario(size_mb, mode, drop_rate=0, corrupt_rate=0, double_rate=0):
    generate_file(size_mb)

    proxy_cmd = [
        "python3",
        PROXY_SCRIPT,
        "-p",
        str(PROXY_PORT),
        "-d",
        str(SERVER_PORT),
        "-m",
        mode,
        "--drop-rate",
        str(drop_rate),
        "--corrupt-rate",
        str(corrupt_rate),
        "--double-rate",
        str(double_rate),
    ]
    proxy_proc = subprocess.Popen(
        proxy_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )
    time.sleep(0.2)

    receiver_cmd = [BINARY, "-r", "-p", str(SERVER_PORT), "-f", RECEIVED_FILE]
    receiver_proc = subprocess.Popen(
        receiver_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )
    time.sleep(0.2)

    start_time = time.perf_counter()

    sender_cmd = [
        BINARY,
        "-s",
        "-H",
        "127.0.0.1",
        "-p",
        str(PROXY_PORT),
        "-f",
        TEST_FILE,
    ]
    sender_result = subprocess.run(
        sender_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )

    end_time = time.perf_counter()

    try:
        receiver_proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        receiver_proc.kill()

    proxy_proc.terminate()
    proxy_proc.wait()

    elapsed = end_time - start_time
    file_size_bytes = size_mb * 1024 * 1024
    speed_mb_s = (file_size_bytes / (1024 * 1024)) / elapsed if elapsed > 0 else 0

    is_success = False
    if os.path.exists(RECEIVED_FILE):
        is_success = calculate_md5(TEST_FILE) == calculate_md5(RECEIVED_FILE)

    if os.path.exists(TEST_FILE):
        os.remove(TEST_FILE)
    if os.path.exists(RECEIVED_FILE):
        os.remove(RECEIVED_FILE)

    impairment_desc = "None"
    if mode == "drop":
        impairment_desc = f"Drop: {drop_rate}%"
    elif mode == "corrupt":
        impairment_desc = f"Corrupt: {corrupt_rate}%"
    elif mode == "double":
        impairment_desc = f"Double: {double_rate}%"

    return {
        "size_mb": size_mb,
        "mode": mode.upper(),
        "impairment": impairment_desc,
        "time_s": elapsed,
        "speed_mb_s": speed_mb_s,
        "success": is_success,
    }


if __name__ == "__main__":
    print("=== Protocol-7 Resilience & Benchmark Suite ===")

    scenarios = [
        {"size": 10, "mode": "normal", "drop": 0, "corrupt": 0, "double": 0},
        {"size": 10, "mode": "drop", "drop": 5, "corrupt": 0, "double": 0},
        {"size": 10, "mode": "drop", "drop": 15, "corrupt": 0, "double": 0},
        {"size": 10, "mode": "corrupt", "drop": 0, "corrupt": 5, "double": 0},
        {"size": 10, "mode": "double", "drop": 0, "corrupt": 0, "double": 10},
    ]

    results = []
    for sc in scenarios:
        print(
            f"\n[TEST] Mode: {sc['mode']} | Size: {sc['size']} MB | Drop={sc['drop']}% Corr={sc['corrupt']}% Dbl={sc['double']}%"
        )
        res = run_benchmark_scenario(
            size_mb=sc["size"],
            mode=sc["mode"],
            drop_rate=sc["drop"],
            corrupt_rate=sc["corrupt"],
            double_rate=sc["double"],
        )
        results.append(res)
        status = "[OK] PASS" if res["success"] else "[FAIL] CORRUPTED"
        print(
            f"       -> Time: {res['time_s']:.3f}s | Speed: {res['speed_mb_s']:.2f} MB/s | Integrity: {status}"
        )

    print("\n\n=== RESULT TABLE ===")
    print(
        f"{'Mode':<10} | {'Size (MB)':<10} | {'Impairment':<20} | {'Time (s)':<10} | {'Speed (MB/s)':<15} | {'Integrity':<10}"
    )
    print("-" * 85)
    for r in results:
        status = "PASS" if r["success"] else "FAIL"
        print(
            f"{r['mode']:<10} | {r['size_mb']:<10} | {r['impairment']:<20} | {r['time_s']:<10.3f} | {r['speed_mb_s']:<15.2f} | {status:<10}"
        )
