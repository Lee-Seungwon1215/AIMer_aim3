#!/usr/bin/env python3
"""Trigger and capture one UART preflight run without third-party packages."""

import argparse
import os
import select
import sys
import termios
import time
import tty


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("port")
    parser.add_argument("--timeout", type=float, default=900.0)
    parser.add_argument("--output")
    args = parser.parse_args()

    fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        tty.setraw(fd)
        attrs = termios.tcgetattr(fd)
        attrs[4] = termios.B115200
        attrs[5] = termios.B115200
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        termios.tcflush(fd, termios.TCIOFLUSH)

        deadline = time.monotonic() + args.timeout
        received = bytearray()
        next_trigger = 0.0
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
            sys.stdout.buffer.write(chunk)
            sys.stdout.buffer.flush()
            normalized = received.replace(b"\r\n", b"\n")
            if any(
                marker in normalized
                for marker in (b"\nDONE=pass\n", b"\nDONE=fail\n", b"\nDONE=fatal\n")
            ):
                break
        else:
            print("capture timeout", file=sys.stderr)
            return 2

        if args.output:
            with open(args.output, "wb") as output:
                output.write(received)
        return 0
    finally:
        os.close(fd)


if __name__ == "__main__":
    raise SystemExit(main())
