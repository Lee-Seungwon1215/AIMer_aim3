#!/usr/bin/env python3
"""Summarize paired Cortex-M7 raw cycles without removing observations."""

import csv
import hashlib
from pathlib import Path
import random
import statistics
import sys


BOOTSTRAP_REPLICATES = 10_000
RUNS = 30
SAMPLES_PER_RUN = 50
N_A_PARAMETERS = ("128s", "192s", "256f", "256s")


def percentile(sorted_values: list[float], probability: float) -> float:
    position = probability * (len(sorted_values) - 1)
    lower = int(position)
    upper = min(lower + 1, len(sorted_values) - 1)
    fraction = position - lower
    return sorted_values[lower] * (1.0 - fraction) + sorted_values[upper] * fraction


def load_rows(path: Path, category: str) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as source:
        rows = list(csv.DictReader(source))
    for row in rows:
        row["category"] = category
    return rows


def paired_interval(reference: list[float], optimized: list[float], key: str) -> tuple[float, float]:
    seed = int.from_bytes(hashlib.sha256(key.encode("utf-8")).digest()[:8], "big")
    generator = random.Random(seed)
    ratios = []
    for _ in range(BOOTSTRAP_REPLICATES):
        indices = [generator.randrange(RUNS) for _ in range(RUNS)]
        ref_median = statistics.median(reference[index] for index in indices)
        opt_median = statistics.median(optimized[index] for index in indices)
        ratios.append(ref_median / opt_median)
    ratios.sort()
    return percentile(ratios, 0.025), percentile(ratios, 0.975)


def format_number(value: float) -> str:
    return f"{value:.6f}"


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: analyze_results.py RESULT_DIR")
    result = Path(sys.argv[1]).resolve()
    rows = (
        load_rows(result / "controls_raw.csv", "control")
        + load_rows(result / "kernel_raw.csv", "kernel")
        + load_rows(result / "e2e_raw.csv", "e2e")
    )

    samples: dict[tuple[str, str, str, str, int], list[int]] = {}
    for row in rows:
        key = (
            row["category"], row["configuration"], row["operation"],
            row["implementation"], int(row["pair_run"]),
        )
        samples.setdefault(key, []).append(int(row["cycles"]))

    for key, values in samples.items():
        if len(values) != SAMPLES_PER_RUN:
            raise RuntimeError(f"{key} has {len(values)} samples, expected 50")

    run_medians = []
    for (category, configuration, operation, implementation, pair_run), values in sorted(samples.items()):
        run_medians.append(
            {
                "category": category,
                "configuration": configuration,
                "operation": operation,
                "implementation": implementation,
                "pair_run": pair_run,
                "median_cycles": statistics.median(values),
            }
        )

    with (result / "run_medians.csv").open("w", newline="", encoding="utf-8") as output:
        columns = ["category", "configuration", "operation", "implementation", "pair_run", "median_cycles"]
        writer = csv.DictWriter(output, fieldnames=columns)
        writer.writeheader()
        writer.writerows(run_medians)

    triples = sorted({(row["category"], row["configuration"], row["operation"]) for row in rows})
    summary = []
    for category, configuration, operation in triples:
        per_impl_all: dict[str, list[int]] = {}
        per_impl_runs: dict[str, list[float]] = {}
        for implementation in ("reference", "optimized"):
            all_values = [
                int(row["cycles"]) for row in rows
                if row["category"] == category
                and row["configuration"] == configuration
                and row["operation"] == operation
                and row["implementation"] == implementation
            ]
            medians = [
                statistics.median(samples[(category, configuration, operation, implementation, pair_run)])
                for pair_run in range(1, RUNS + 1)
            ]
            if len(all_values) != RUNS * SAMPLES_PER_RUN or len(medians) != RUNS:
                raise RuntimeError(f"incomplete group: {category}/{configuration}/{operation}/{implementation}")
            per_impl_all[implementation] = all_values
            per_impl_runs[implementation] = medians

        reference_median = statistics.median(per_impl_all["reference"])
        optimized_median = statistics.median(per_impl_all["optimized"])
        reference_stddev = statistics.stdev(per_impl_runs["reference"])
        optimized_stddev = statistics.stdev(per_impl_runs["optimized"])
        reference_mean = statistics.mean(per_impl_runs["reference"])
        optimized_mean = statistics.mean(per_impl_runs["optimized"])
        reference_sample_stddev = statistics.stdev(per_impl_all["reference"])
        optimized_sample_stddev = statistics.stdev(per_impl_all["optimized"])
        reference_sample_mean = statistics.mean(per_impl_all["reference"])
        optimized_sample_mean = statistics.mean(per_impl_all["optimized"])
        speedup = reference_median / optimized_median
        ci_low, ci_high = paired_interval(
            per_impl_runs["reference"], per_impl_runs["optimized"],
            f"{category}/{configuration}/{operation}",
        )
        summary.append(
            {
                "category": category,
                "configuration": configuration,
                "operation": operation,
                "status": "measured",
                "runs": RUNS,
                "samples_per_run": SAMPLES_PER_RUN,
                "samples_per_implementation": RUNS * SAMPLES_PER_RUN,
                "reference_median_cycles": format_number(reference_median),
                "optimized_median_cycles": format_number(optimized_median),
                "reference_run_median_stddev": format_number(reference_stddev),
                "optimized_run_median_stddev": format_number(optimized_stddev),
                "reference_run_median_cv_percent": format_number(100.0 * reference_stddev / reference_mean),
                "optimized_run_median_cv_percent": format_number(100.0 * optimized_stddev / optimized_mean),
                "reference_all_sample_stddev": format_number(reference_sample_stddev),
                "optimized_all_sample_stddev": format_number(optimized_sample_stddev),
                "reference_all_sample_cv_percent": format_number(100.0 * reference_sample_stddev / reference_sample_mean),
                "optimized_all_sample_cv_percent": format_number(100.0 * optimized_sample_stddev / optimized_sample_mean),
                "reference_over_optimized": format_number(speedup),
                "cycle_reduction_percent": format_number(100.0 * (1.0 - optimized_median / reference_median)),
                "paired_bootstrap_95ci_low": format_number(ci_low),
                "paired_bootstrap_95ci_high": format_number(ci_high),
                "bootstrap_replicates": BOOTSTRAP_REPLICATES,
            }
        )

    for parameter in N_A_PARAMETERS:
        for operation in ("sign", "verify"):
            summary.append(
                {
                    "category": "e2e",
                    "configuration": parameter,
                    "operation": operation,
                    "status": "N/A (insufficient SRAM)",
                    "runs": 0,
                    "samples_per_run": 0,
                    "samples_per_implementation": 0,
                    "reference_median_cycles": "",
                    "optimized_median_cycles": "",
                    "reference_run_median_stddev": "",
                    "optimized_run_median_stddev": "",
                    "reference_run_median_cv_percent": "",
                    "optimized_run_median_cv_percent": "",
                    "reference_all_sample_stddev": "",
                    "optimized_all_sample_stddev": "",
                    "reference_all_sample_cv_percent": "",
                    "optimized_all_sample_cv_percent": "",
                    "reference_over_optimized": "",
                    "cycle_reduction_percent": "",
                    "paired_bootstrap_95ci_low": "",
                    "paired_bootstrap_95ci_high": "",
                    "bootstrap_replicates": 0,
                }
            )

    summary.sort(key=lambda row: (row["category"], row["configuration"], row["operation"]))
    columns = [
        "category", "configuration", "operation", "status", "runs",
        "samples_per_run", "samples_per_implementation",
        "reference_median_cycles", "optimized_median_cycles",
        "reference_run_median_stddev", "optimized_run_median_stddev",
        "reference_run_median_cv_percent", "optimized_run_median_cv_percent",
        "reference_all_sample_stddev", "optimized_all_sample_stddev",
        "reference_all_sample_cv_percent", "optimized_all_sample_cv_percent",
        "reference_over_optimized", "cycle_reduction_percent",
        "paired_bootstrap_95ci_low", "paired_bootstrap_95ci_high",
        "bootstrap_replicates",
    ]
    with (result / "summary.csv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=columns)
        writer.writeheader()
        writer.writerows(summary)
    print(f"wrote {len(summary)} summary rows and {len(run_medians)} run medians")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
