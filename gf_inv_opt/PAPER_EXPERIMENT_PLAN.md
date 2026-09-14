# AIMer v3 GF Inversion Paper and Experiment Plan

## 1. Scope

This work evaluates a portable optimization of `gf_inv()` in the AIMer v3
reference implementation.

- `../Reference_Implementation` is the unchanged baseline.
- `gf_inv_opt` is the candidate implementation.
- Only the inversion schedule in `field_common.c` differs.
- `gf_mul()`, `gf_sqr()`, AIMer v3 parameters, and the signature algorithm
  remain unchanged.
- No architecture-specific intrinsic is introduced by the optimization.
- S-box intermediate-value reuse is outside the scope of this work.

The paper must not claim a new inversion algorithm. The implementation applies
fixed addition chains following the Itoh--Tsujii approach to AIMer v3's three
binary-field sizes.

## 2. Core Claim

The mathematical definition of inversion remains

```text
a^(-1) = a^(2^m - 2).
```

Let `A_k = a^(2^k - 1)`. Previously computed values are combined using

```text
A_(i+j) = A_i^(2^j) * A_j.
```

This reorganizes the multiplication schedule around inexpensive repeated
squaring. It does not increase the number of field squarings in the current
implementation; it reduces the number of general field multiplications.

| Field | Baseline operations | Fixed-chain operations |
|---|---:|---:|
| GF(2^128) | 127S + 127M | 127S + 10M |
| GF(2^192) | 191S + 191M | 191S + 11M |
| GF(2^256) | 255S + 255M | 255S + 10M |

`S` denotes a field squaring and `M` denotes a general field multiplication.
These are algorithm-level calls, not retired machine-instruction counts.

The implementation is architecture-independent at the source level. Actual
speedup is hardware-dependent because the relative costs of `gf_mul()` and
`gf_sqr()` differ by platform.

## 3. Evaluation Platforms

Use two complementary platforms:

1. A high-performance x86-64 processor.
2. An embedded Arm Cortex-M7 microcontroller.

Absolute cycle counts must not be compared between the two platforms. Report
the baseline-to-candidate speedup separately on each platform.

## 4. Build Conditions

For each platform, compile the baseline and candidate with the same compiler,
compiler version, flags, source dependencies, and linker configuration.

### x86-64

- C compiler: record the exact GCC or Clang version.
- Required flags: `-O3 -std=c11 -fomit-frame-pointer`.
- Do not use hand-written SIMD intrinsics for this experiment.
- Disable LTO for both implementations.
- If a target-specific option such as `-march=native` is used, apply it to both
  implementations and record it explicitly.

### Cortex-M7

- Toolchain: record the exact `arm-none-eabi-gcc` version.
- Required optimization and target flags: `-O3 -std=c11 -mcpu=cortex-m7
  -mthumb` together with the board's required ABI options.
- Record the exact board and MCU model, clock frequency, Flash and SRAM sizes,
  and any enabled instruction and data caches.
- Apply the board-specific `-mfpu` and `-mfloat-abi` options identically to
  both implementations. Do not enable an FPU option unsupported by the MCU.
- Use the same linker script and memory placement for both implementations.
- Do not use hand-written DSP/SIMD intrinsics or assembly for this experiment.
- Disable LTO for both implementations.

Compiler auto-vectorization is allowed only under identical baseline and
candidate flags. Preserve the generated assembly or disassembly as an
experimental artifact.

## 5. Correctness Gate

Complete correctness validation before performance measurement.

1. Run the official KAT for all six parameter sets.
2. Compare baseline and candidate inversions on randomly generated nonzero
   elements in GF(2^128), GF(2^192), and GF(2^256).
3. Verify `a * gf_inv(a) = 1` for every nonzero test input.
4. Confirm that zero input behaves identically in both implementations.
5. Run key generation, signing, verification, and tampered-signature tests for
   all six parameter sets.

Any mismatch is a stop condition. Do not collect or report performance results
from a failing implementation.

## 6. Measurement Conditions

### Common rules

- Generate benchmark inputs before entering the timed region.
- Use the same fixed input set and seed for baseline and candidate.
- Prevent dead-code elimination with a consumed output checksum.
- Warm up code and data before recorded warm-cache measurements.
- Do not remove outliers after observing the results.
- Alternate measurement order to reduce drift: A-B, B-A, A-B, and so on.

### x86-64

- Pin the process to one physical core.
- Use a fixed-frequency performance configuration.
- Disable Turbo Boost for the complete measurement session.
- Minimize background activity.
- Use a serialized cycle counter or a documented PMU method.

### Cortex-M7

- Use a single-core bare-metal configuration.
- Fix the processor clock and disable dynamic frequency changes.
- Use identical instruction/data-cache, TCM, flash wait-state, and
  code/data-placement settings.
- Record whether code executes from Flash or ITCM and whether benchmark data
  resides in SRAM or DTCM.
- Disable interrupts within the timed region.
- Use the DWT `CYCCNT` cycle counter and document its initialization.
- Keep each timed interval below the 32-bit counter's wraparound limit.

## 7. Repetition and Statistics

- Perform 30 independent runs per reported configuration.
- Collect at least 50 timed samples in each run.
- Calibrate inner repetitions so timer overhead is negligible.
- Use the median cycle count as the primary result.
- Report run-level variability using CV or standard deviation.
- Report a 95% bootstrap confidence interval for speedup when possible.

Calculate speedup independently on each platform:

```text
speedup = median(baseline cycles) / median(candidate cycles).
```

## 8. Metrics

### Operation counts

- Number of `gf_mul()` calls per inversion.
- Number of `gf_sqr()` calls per inversion.

Confirm these values using a separate instrumented build. Do not use the
instrumented build for cycle measurements.

### Kernel measurements

- `gf_mul()` cycles.
- `gf_sqr()` cycles.
- `gf_inv()` cycles.
- Retired instructions when supported by the platform.

Since `gf_mul()` and `gf_sqr()` are unchanged, their baseline and candidate
measurements should agree within normal measurement variation.

### End-to-end measurements

For `128f`, `128s`, `192f`, `192s`, `256f`, and `256s`, measure:

- key generation;
- signing with a fixed message length;
- verification of a valid signature.

Also record binary text size and maximum stack usage, particularly on
Cortex-M7. Use a separate instrumented build to count `gf_inv()` calls in each
end-to-end operation so the kernel result can be related to the observed
end-to-end speedup.

## 9. Paper Wording

Preferred claim:

> We replace AIMer v3's sequential inversion schedule with
> parameter-specific fixed addition chains. The implementation preserves the
> field definition and the underlying multiplication and squaring routines,
> while reducing general field multiplications from 127, 191, and 255 to 10,
> 11, and 10 for GF(2^128), GF(2^192), and GF(2^256), respectively. Because
> the optimization requires no architecture-specific instruction, the same C
> source is evaluated on a high-performance x86-64 processor and an embedded
> Cortex-M7 microcontroller.

Do not claim that the optimization is performance-independent or faster on all
hardware. The acceptable conclusion is that it is portable at the source level
and improved performance on the evaluated platforms.
