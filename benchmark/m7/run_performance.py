#!/usr/bin/env python3
"""Run resumable paired Cortex-M7 measurements and materialize raw CSV files."""

import argparse
import csv
import os
from pathlib import Path
import re
import select
import subprocess
import sys
import termios
import time
import tty


OPENOCD = Path("/private/tmp/aimer-m7-tools/openocd/bin/openocd")
STLINK_SERIAL = "066AFF565380535067105130"
UART_PORT = "/dev/cu.usbmodem11403"
SAMPLE_RE = re.compile(rb"^SAMPLE,([^,]+),(\d+),(\d+)$", re.MULTILINE)
VALUE_RE = re.compile(rb"^([^=\r\n]+)=([^\r\n]+)$", re.MULTILINE)
FIELDS = ("128", "192", "256")
PARAMETERS = ("128f", "128s", "192f", "192s", "256f", "256s")
IMPLEMENTATIONS = ("reference", "optimized")


def capture_serial(output: Path, timeout: float) -> bytes:
    fd = os.open(UART_PORT, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        tty.setraw(fd)
        attrs = termios.tcgetattr(fd)
        attrs[4] = termios.B115200
        attrs[5] = termios.B115200
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        termios.tcflush(fd, termios.TCIOFLUSH)

        deadline = time.monotonic() + timeout
        next_trigger = 0.0
        received = bytearray()
        while time.monotonic() < deadline:
            now = time.monotonic()
            if now >= next_trigger:
                os.write(fd, b"R")
                next_trigger = now + 0.5
            readable, _, _ = select.select([fd], [], [], 0.25)
            if not readable:
                continue
            try:
                chunk = os.read(fd, 4096)
            except BlockingIOError:
                continue
            if not chunk:
                continue
            received.extend(chunk)
            normalized = received.replace(b"\r\n", b"\n")
            if any(
                marker in normalized
                for marker in (b"\nDONE=pass\n", b"\nDONE=fail\n", b"\nDONE=fatal\n")
            ):
                output.write_bytes(received)
                return bytes(received)
        output.write_bytes(received)
        raise RuntimeError(f"UART timeout after {timeout}s: {output}")
    finally:
        os.close(fd)


def parse_log(data: bytes, expected_samples: int) -> tuple[dict[str, str], list[tuple[str, int, int]]]:
    data = data.replace(b"\r\n", b"\n")
    values = {
        match.group(1).decode("ascii"): match.group(2).decode("ascii")
        for match in VALUE_RE.finditer(data)
    }
    samples = [
        (match.group(1).decode("ascii"), int(match.group(2)), int(match.group(3)))
        for match in SAMPLE_RE.finditer(data)
    ]
    if values.get("BENCH_RESULT") != "PASS" or b"DONE=pass" not in data:
        raise RuntimeError("benchmark firmware did not report PASS")
    if len(samples) != expected_samples:
        raise RuntimeError(f"expected {expected_samples} samples, got {len(samples)}")
    for _, sample, cycles in samples:
        if sample < 0 or sample >= 50 or cycles <= 0 or cycles >= 2**32:
            raise RuntimeError(f"invalid DWT sample index/cycles: {sample}/{cycles}")
    return values, samples


def flash_and_capture(elf: Path, flash_log: Path, uart_log: Path,
                      expected_samples: int, timeout: float) -> tuple[dict[str, str], list[tuple[str, int, int]]]:
    if uart_log.exists():
        try:
            return parse_log(uart_log.read_bytes(), expected_samples)
        except RuntimeError:
            pass

    flash_log.parent.mkdir(parents=True, exist_ok=True)
    with flash_log.open("wb") as output:
        subprocess.run(
            [
                str(OPENOCD), "-f", "board/st_nucleo_f7.cfg",
                "-c", f"adapter serial {STLINK_SERIAL}",
                "-c", "init", "-c", "reset halt",
                "-c", f"program {elf} verify reset exit",
            ],
            stdout=output,
            stderr=subprocess.STDOUT,
            check=True,
        )
    data = capture_serial(uart_log, timeout)
    return parse_log(data, expected_samples)


def order_for_run(pair_run: int) -> tuple[str, str]:
    return IMPLEMENTATIONS if pair_run % 2 == 1 else tuple(reversed(IMPLEMENTATIONS))


def run_one_pair(repo: Path, result: Path, kind: str, configuration: str,
                 pair_run: int) -> None:
    order = order_for_run(pair_run)
    order_label = "A-B" if order[0] == "reference" else "B-A"
    expected_samples = 150 if kind == "kernel" else (150 if configuration in ("128f", "192f") else 50)
    checksums: dict[str, str] = {}

    for position, implementation in enumerate(order, start=1):
        parameter = f"{configuration}f" if kind == "kernel" else configuration
        build = repo / "benchmark" / "m7" / "build" / f"{kind}-{implementation}-{parameter}"
        elf = build / f"{kind}.elf"
        prefix = result / "raw_logs" / kind / configuration / f"pair-{pair_run:02d}-pos-{position}-{implementation}"
        print(
            f"{time.strftime('%Y-%m-%d %H:%M:%S')} {kind} {configuration} "
            f"pair {pair_run:02d}/30 {order_label} position {position} {implementation}",
            flush=True,
        )
        values, _ = flash_and_capture(
            elf, prefix.with_name(prefix.name + "-flash.log"),
            prefix.with_name(prefix.name + "-uart.log"), expected_samples, 1800.0,
        )
        checksums[implementation] = values["checksum"]

    if checksums["reference"] != checksums["optimized"]:
        raise RuntimeError(
            f"checksum mismatch for {kind}/{configuration}/pair {pair_run}: {checksums}"
        )


def collect_rows(result: Path, kind: str, configurations: tuple[str, ...], runs: int) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for pair_run in range(1, runs + 1):
        order = order_for_run(pair_run)
        order_label = "A-B" if order[0] == "reference" else "B-A"
        for configuration in configurations:
            expected_samples = 150 if kind == "kernel" else (150 if configuration in ("128f", "192f") else 50)
            for position, implementation in enumerate(order, start=1):
                prefix = result / "raw_logs" / kind / configuration / f"pair-{pair_run:02d}-pos-{position}-{implementation}-uart.log"
                values, samples = parse_log(prefix.read_bytes(), expected_samples)
                for operation, sample, cycles in samples:
                    rows.append(
                        {
                            "pair_run": pair_run,
                            "order": order_label,
                            "order_position": position,
                            "implementation": implementation,
                            "configuration": configuration,
                            "operation": operation,
                            "sample": sample,
                            "cycles": cycles,
                            "checksum": values["checksum"],
                        }
                    )
    return rows


def write_csv(path: Path, rows: list[dict[str, object]], operations: set[str]) -> None:
    selected = [row for row in rows if str(row["operation"]) in operations]
    columns = [
        "pair_run", "order", "order_position", "implementation",
        "configuration", "operation", "sample", "cycles", "checksum",
    ]
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=columns)
        writer.writeheader()
        writer.writerows(selected)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("result_dir", type=Path)
    parser.add_argument("--runs", type=int, default=30)
    args = parser.parse_args()
    if args.runs != 30:
        print("warning: final protocol requires exactly 30 runs", file=sys.stderr)

    repo = Path(__file__).resolve().parents[2]
    result = args.result_dir.resolve()
    result.mkdir(parents=True, exist_ok=True)

    for pair_run in range(1, args.runs + 1):
        for field in FIELDS:
            run_one_pair(repo, result, "kernel", field, pair_run)
        for parameter in PARAMETERS:
            run_one_pair(repo, result, "e2e", parameter, pair_run)

    kernel_rows = collect_rows(result, "kernel", FIELDS, args.runs)
    e2e_rows = collect_rows(result, "e2e", PARAMETERS, args.runs)
    write_csv(result / "controls_raw.csv", kernel_rows, {"gf_mul", "gf_sqr"})
    write_csv(result / "kernel_raw.csv", kernel_rows, {"gf_inv"})
    write_csv(result / "e2e_raw.csv", e2e_rows, {"keypair", "sign", "verify"})
    print("all paired measurements complete", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
