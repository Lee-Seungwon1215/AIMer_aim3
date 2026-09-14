#!/usr/bin/env python3
"""Collect ELF/disassembly, code size, and compiler stack reports."""

import csv
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Union


TOOL_ROOT = Path("/private/tmp/aimer-m7-tools/gcc/bin")
SIZE = TOOL_ROOT / "arm-none-eabi-size"
NM = TOOL_ROOT / "arm-none-eabi-nm"


def symbol_sizes(elf: Path) -> dict[str, int]:
    output = subprocess.check_output([str(NM), "-S", "--size-sort", str(elf)], text=True)
    sizes = {}
    for line in output.splitlines():
        parts = line.split()
        if len(parts) >= 4:
            try:
                sizes[parts[3]] = int(parts[1], 16)
            except ValueError:
                pass
    return sizes


def suffix_size(sizes: dict[str, int], suffix: str) -> Union[int, str]:
    matches = [size for symbol, size in sizes.items() if symbol.endswith(suffix)]
    return matches[0] if len(matches) == 1 else ""


def binary_sizes(elf: Path) -> tuple[int, int, int, int]:
    output = subprocess.check_output([str(SIZE), "-B", str(elf)], text=True)
    parts = output.splitlines()[-1].split()
    return tuple(int(value) for value in parts[:4])


def stack_entries(build: Path) -> list[tuple[str, int, str]]:
    entries = []
    for report in sorted(build.rglob("*.su")):
        for line in report.read_text(encoding="utf-8").splitlines():
            parts = line.rsplit("\t", 2)
            if len(parts) != 3:
                continue
            location_and_function, frame, qualifier = parts
            function = location_and_function.rsplit(":", 1)[-1]
            try:
                entries.append((function, int(frame), qualifier))
            except ValueError:
                continue
    return entries


def suffix_frame(entries: list[tuple[str, int, str]], suffix: str) -> Union[int, str]:
    matches = [frame for function, frame, _ in entries if function.endswith(suffix)]
    return matches[0] if len(matches) == 1 else ""


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: collect_artifacts.py RESULT_DIR")
    m7 = Path(__file__).resolve().parent
    result = Path(sys.argv[1]).resolve()
    artifacts = result / "disassembly"
    stack_root = result / "stack_reports"
    artifacts.mkdir(parents=True, exist_ok=True)
    stack_root.mkdir(parents=True, exist_ok=True)

    builds = []
    for field in ("128", "192", "256"):
        builds.append(("correctness", "paired", field, f"correctness-field-{field}f", "correctness"))
    for implementation in ("reference", "optimized"):
        for field in ("128", "192", "256"):
            builds.append(("kernel", implementation, field, f"kernel-{implementation}-{field}f", "kernel"))
        for parameter in ("128f", "128s", "192f", "192s", "256f", "256s"):
            builds.append(("e2e", implementation, parameter, f"e2e-{implementation}-{parameter}", "e2e"))

    code_rows = []
    stack_rows = []
    for kind, implementation, configuration, tag, stem in builds:
        build = m7 / "build" / tag
        elf = build / f"{stem}.elf"
        for extension in ("elf", "bin", "map", "disasm.txt"):
            source = build / f"{stem}.{extension}"
            shutil.copy2(source, artifacts / f"{tag}.{extension}")

        destination = stack_root / tag
        for report in build.rglob("*.su"):
            relative = report.relative_to(build)
            target = destination / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(report, target)

        text, data, bss, total = binary_sizes(elf)
        symbols = symbol_sizes(elf)
        entries = stack_entries(build)
        largest_function, largest_frame, largest_qualifier = max(entries, key=lambda entry: entry[1])
        code_rows.append(
            {
                "kind": kind,
                "implementation": implementation,
                "configuration": configuration,
                "text_bytes": text,
                "data_bytes": data,
                "bss_bytes": bss,
                "total_dec_bytes": total,
                "gf_inv_bytes": suffix_size(symbols, "_gf_inv"),
                "gf_mul_bytes": suffix_size(symbols, "_gf_mul"),
                "gf_sqr_bytes": suffix_size(symbols, "_gf_sqr"),
                "keypair_bytes": suffix_size(symbols, "_crypto_sign_keypair"),
                "sign_bytes": suffix_size(symbols, "_crypto_sign_signature"),
                "verify_bytes": suffix_size(symbols, "_crypto_sign_verify"),
                "artifact": f"disassembly/{tag}.elf",
            }
        )
        stack_rows.append(
            {
                "kind": kind,
                "implementation": implementation,
                "configuration": configuration,
                "largest_single_frame_bytes": largest_frame,
                "largest_single_frame_function": largest_function,
                "largest_frame_qualifier": largest_qualifier,
                "gf_inv_frame_bytes": suffix_frame(entries, "_gf_inv"),
                "gf_mul_frame_bytes": suffix_frame(entries, "_gf_mul"),
                "gf_sqr_frame_bytes": suffix_frame(entries, "_gf_sqr"),
                "keypair_frame_bytes": suffix_frame(entries, "_crypto_sign_keypair"),
                "sign_frame_bytes": suffix_frame(entries, "_crypto_sign_signature"),
                "verify_frame_bytes": suffix_frame(entries, "_crypto_sign_verify"),
                "status": "compiler_static_individual_frames",
            }
        )

    code_columns = list(code_rows[0])
    with (result / "code_size.csv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=code_columns)
        writer.writeheader()
        writer.writerows(code_rows)

    stack_columns = list(stack_rows[0])
    with (result / "stack_usage.csv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=stack_columns)
        writer.writeheader()
        writer.writerows(stack_rows)
    print(f"collected {len(builds)} builds")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
