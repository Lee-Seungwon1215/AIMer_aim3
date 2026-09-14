#!/usr/bin/env python3
"""Write a deterministic SHA256SUMS file for one result directory."""

import hashlib
from pathlib import Path
import sys


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: write_checksums.py RESULT_DIR")
    result = Path(sys.argv[1]).resolve()
    output = result / "SHA256SUMS"
    files = sorted(path for path in result.rglob("*") if path.is_file() and path != output)
    lines = []
    for path in files:
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        lines.append(f"{digest}  ./{path.relative_to(result).as_posix()}\n")
    output.write_text("".join(lines), encoding="utf-8")
    print(f"wrote {len(lines)} checksums")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
