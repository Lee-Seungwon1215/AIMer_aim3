# AIMer v3 Cortex-M7 `gf_inv` fixed addition-chain experiment

## Status

**Completed.** The continuation correctness gate passed and all requested
executable measurements were collected. The earlier stopped experiment remains
unchanged under `benchmark/results/m7-gf-inv-20260914-120916/`.

The final data contain 30 independent paired runs, 50 samples per operation and
implementation, and 57,000 raw DWT cycle samples. No observation was removed.

## Board and build environment

| Item | Observed/configured value |
|---|---|
| Board | ST NUCLEO-F767ZI (board/package inferred from Nucleo wiring and silicon evidence) |
| MCU | STM32F767ZIT6; STM32F76x/77x device ID `0x451` |
| Core | Cortex-M7 r1p0, CPUID `0x411fc270` |
| Clock | Fixed 160 MHz: HSI 16 MHz, PLL `M=8`, `N=160`, `P=2` |
| Flash | 2 MiB internal Flash; code at `0x08000000` |
| SRAM | 512 KiB; `.data`/`.bss`/heap at `0x20000000`, stack top `0x20080000` |
| Cache | 16 KiB I-cache and 16 KiB D-cache enabled (`SCB_CCR=0x00070200`) |
| TCM | 16 KiB ITCM exists but is unused for code; 128 KiB DTCM is in the linked data range |
| Flash access | Prefetch and ART enabled, 7 wait states (`FLASH_ACR=0x00000307`) |
| Compiler | xPack `arm-none-eabi-gcc` 15.2.1 20251203 |
| Flags | `-O3 -std=c11 -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard -fno-lto` |
| Debug/flashing | ST-LINK/V2-1 V2J39M27; xPack OpenOCD 0.12.0 development build |
| Linker script | `benchmark/m7/stm32f767zi.ld` |
| Execution | Single-core bare metal; interrupts disabled; no dynamic clock change |
| Cycle counter | DWT `CYCCNT`, unlocked and verified |

Reference and optimized builds used the same source dependency list, flags,
linker script, memory regions, allocator, inputs, and seeds. The larger optimized
function naturally changes some intra-image function addresses, but both images
use the same Flash and SRAM regions.

## Correctness gate

| Test | Result |
|---|---|
| Official KAT, all six parameters | NOT RUN; excluded from the continuation correctness gate after the earlier SRAM-stop condition was lifted |
| GF128 Reference/optimized differential, fixed nonzero inputs | PASS, 4,096 inputs |
| GF192 Reference/optimized differential, fixed nonzero inputs | PASS, 4,096 inputs |
| GF256 Reference/optimized differential, fixed nonzero inputs | PASS, 4,096 inputs |
| `a * gf_inv(a) = 1`, both implementations and all fields | PASS |
| In-place inversion, both implementations and all fields | PASS |
| Zero-input behavior and zero result | PASS |
| 128f keypair/sign/verify/tamper, both implementations | PASS; checksum `0x61f27d12` matched |
| 192f keypair/sign/verify/tamper, both implementations | PASS; checksum `0x6405ca3c` matched |
| All 270 performance-run checksum pairs | PASS |

Field-test checksums were GF128 `0x80f63d99`, GF192 `0xd9cb13d2`, and
GF256 `0xa17fe731`. No output mismatch was observed.

## Measurement and statistics

- Odd paired runs used Reference then optimized (A-B); even runs used optimized
  then Reference (B-A). Each run reflashed and reset the target.
- Kernel inputs were generated before timing. The fixed message and DRBG seed
  were initialized before E2E timing; unmodified API-internal randomness remains
  part of keypair/sign time.
- Kernel warm-up was 100 calls per operation. E2E warm-up was 3 calls per
  operation.
- UART and checksum processing occurred outside the DWT intervals.
- DWT barrier/read overhead was retained equally in both builds and was not
  subtracted.
- The maximum observed interval was 92,653,150 cycles (0.579 s), safely below
  the 32-bit wrap period at 160 MHz.
- The representative value is the median of all 1,500 samples per implementation.
  Tables also show the standard deviation and CV over all raw samples.
- The 30 run medians were identical for every measured row, so their SD and CV
  are both zero. The 10,000-replicate bootstrap resampled paired run medians;
  consequently every 95% ratio interval collapsed to the displayed point value.
  This reflects deterministic fixed-input bare-metal execution and does not
  account for systematic platform uncertainty.

## `gf_inv` results

`R/O` is `median(reference) / median(optimized)`. SD and CV use all 1,500 raw
samples for that implementation.

| Field | Reference median | Optimized median | Ref SD / CV | Opt SD / CV | R/O | Cycle reduction | Paired bootstrap 95% CI |
|---|---:|---:|---:|---:|---:|---:|---:|
| GF128 | 257,845 | 30,485 | 3.221 / 0.001249% | 8.043 / 0.026380% | 8.458094x | 88.177006% | [8.458094, 8.458094] |
| GF192 | 768,430 | 67,739 | 2.521 / 0.000328% | 9.875 / 0.014577% | 11.343982x | 91.184753% | [11.343982, 11.343982] |
| GF256 | 1,545,853 | 106,626 | 2.241 / 0.000145% | 1.400 / 0.001313% | 14.497899x | 93.102449% | [14.497899, 14.497899] |

## Control kernels

The `gf_mul` and `gf_sqr` sources and generated function sizes are unchanged.

| Field | Operation | Reference median | Optimized median | Ref SD / CV | Opt SD / CV | R/O | Paired bootstrap 95% CI |
|---|---|---:|---:|---:|---:|---:|---:|
| GF128 | `gf_mul` | 1,964 | 1,967 | 0.722 / 0.036740% | 0.337 / 0.017140% | 0.998475x | [0.998475, 0.998475] |
| GF128 | `gf_sqr` | 102 | 106 | 0.722 / 0.706509% | 0.238 / 0.223992% | 0.962264x | [0.962264, 0.962264] |
| GF192 | `gf_mul` | 3,926 | 3,926 | 0.238 / 0.006051% | 0.238 / 0.006051% | 1.000000x | [1.000000, 1.000000] |
| GF192 | `gf_sqr` | 144 | 144 | 0.140 / 0.097268% | 0.140 / 0.097268% | 1.000000x | [1.000000, 1.000000] |
| GF256 | `gf_mul` | 5,931 | 5,940 | 0.337 / 0.005685% | 0.311 / 0.005229% | 0.998485x | [0.998485, 0.998485] |
| GF256 | `gf_sqr` | 202 | 200 | 0.196 / 0.097023% | 0.311 / 0.155247% | 1.010000x | [1.010000, 1.010000] |

The largest relative control difference is four cycles in the very short GF128
square. Since the source and function size are identical, this is attributed to
intra-Flash address/alignment and fixed DWT barrier/read effects, not to an
arithmetic optimization. The multiplication controls differ by at most 0.153%.

## End-to-end results

| Parameter | Operation | Reference median | Optimized median | Ref SD / CV | Opt SD / CV | R/O | Reduction | Paired bootstrap 95% CI |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 128f | keypair | 2,234,595 | 1,547,702 | 1,013.217 / 0.045344% | 621.636 / 0.040167% | 1.443815x | 30.739038% | [1.443815, 1.443815] |
| 128f | sign | 32,254,333 | 31,868,704.5 | 2,748.761 / 0.008522% | 3,843.767 / 0.012061% | 1.012101x | 1.195587% | [1.012101, 1.012101] |
| 128f | verify | 29,651,162 | 29,703,147 | 354.807 / 0.001197% | 659.451 / 0.002220% | 0.998250x | -0.175322% | [0.998250, 0.998250] |
| 128s | keypair | 2,230,109.5 | 1,542,244 | 421.176 / 0.018885% | 152.824 / 0.009909% | 1.446016x | 30.844472% | [1.446016, 1.446016] |
| 128s | sign / verify | N/A | N/A | — | — | — | — | insufficient SRAM |
| 192f | keypair | 6,653,766 | 4,543,542.5 | 1,828.127 / 0.027475% | 1,143.527 / 0.025168% | 1.464445x | 31.714724% | [1.464445, 1.464445] |
| 192f | sign | 92,644,955 | 91,238,605.5 | 3,391.088 / 0.003660% | 2,886.108 / 0.003163% | 1.015414x | 1.517999% | [1.015414, 1.015414] |
| 192f | verify | 83,983,138 | 84,029,702.5 | 1,335.948 / 0.001591% | 1,274.928 / 0.001517% | 0.999446x | -0.055445% | [0.999446, 0.999446] |
| 192s | keypair | 6,671,812 | 4,534,152 | 1,262.702 / 0.018926% | 482.018 / 0.010631% | 1.471458x | 32.040171% | [1.471458, 1.471458] |
| 192s | sign / verify | N/A | N/A | — | — | — | — | insufficient SRAM |
| 256f | keypair | 19,147,562.5 | 13,400,754 | 2,633.795 / 0.013755% | 1,200.513 / 0.008958% | 1.428842x | 30.013264% | [1.428842, 1.428842] |
| 256f | sign / verify | N/A | N/A | — | — | — | — | insufficient SRAM |
| 256s | keypair | 19,147,562.5 | 13,400,754 | 2,633.795 / 0.013755% | 1,200.513 / 0.008958% | 1.428842x | 30.013264% | [1.428842, 1.428842] |
| 256s | sign / verify | N/A | N/A | — | — | — | — | insufficient SRAM |

The unchanged signing arenas require 1,508,504 B (128s), 3,287,448 B (192s),
977,464 B (256f), and 7,130,808 B (256s), versus 524,288 B available. No
low-memory implementation was introduced.

## Attribution

Key generation executes three inversions for GF128/GF192 and four for GF256.
The observed keypair cycle reductions of approximately 30–32% are consistent
with the isolated inversion savings. Signing executes two inversions for the
measured GF128/GF192 configurations, so its total improvement is diluted to
1.20–1.52% by the much larger MPC/hash workload.

Verification does not call `gf_inv`. Its -0.175% (128f) and -0.055% (192f)
changes are therefore not attributed to the fixed chain; they are consistent
with the small code-placement effects also visible in the controls.

## Code size and stack trade-off

| Field | Kernel text Ref → Opt | `gf_inv` size Ref → Opt | `gf_inv` frame Ref → Opt | Largest E2E individual frame |
|---|---:|---:|---:|---:|
| GF128 | 6,912 → 7,328 B (+416) | 66 → 484 B (+418) | 32 → 96 B (+64) | 4,200 B |
| GF192 | 8,048 → 8,656 B (+608) | 82 → 692 B (+610) | 40 → 152 B (+112) | 9,336 B |
| GF256 | 9,280 → 9,816 B (+536) | 92 → 630 B (+538) | 48 → 192 B (+144) | 16,512 B |

The largest E2E frame is `squeeze_invertible_matrix` and is identical in both
builds. Stack values are compiler-reported individual static frames, not runtime
call-chain high-water measurements. Complete `.su` reports are preserved
locally under `stack_reports/` and are not part of the published result subset.

Whole E2E text sizes increased by 416 B for GF128, 608 B for GF192, and 536 B
for GF256. This closely tracks the isolated `gf_inv` code growth; linker
relaxation accounts for the two-byte difference.

## Published artifacts and local-only scope

- `controls_raw.csv`: 18,000 raw samples
- `kernel_raw.csv`: 9,000 raw samples
- `e2e_raw.csv`: 30,000 raw samples
- `run_medians.csv`: every run-level median
- `summary.csv`: medians, SD, CV, ratios, reductions, and bootstrap intervals
- `code_size.csv` and `stack_usage.csv`: resource summaries
- `memory_preflight.csv`: measured SRAM feasibility data
- `metadata.txt` and `correctness.txt`: sanitized environment and correctness summaries
- `SHA256SUMS`: hashes for this published result subset only

Raw UART/serial logs, OpenOCD/flash logs, ELF/BIN/MAP files, full
disassembly, compiler `.su` files, and build artifacts remain local and are
excluded from the published result subset.

The smoke run is preserved separately and excluded from every statistic. No
outlier removal, low-memory rewrite, algorithm change, DSP/SIMD intrinsic,
assembly optimization, or post-result tuning was performed. Neither
`Reference_Implementation` nor the existing optimized addition chain was
modified. The x86 measurement code/results were not changed. No commit or
GitHub push was made at measurement time; only the sanitized result subset
described above was later prepared for publication.
