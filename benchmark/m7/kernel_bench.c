#include "platform.h"

#include "field.h"

#include <stddef.h>
#include <stdint.h>

#define SAMPLE_COUNT 50u
#define WARMUP_COUNT 100u

static gf inputs_a[SAMPLE_COUNT];
static gf inputs_b[SAMPLE_COUNT];
static gf outputs[SAMPLE_COUNT];
static uint32_t mul_cycles[SAMPLE_COUNT];
static uint32_t sqr_cycles[SAMPLE_COUNT];
static uint32_t inv_cycles[SAMPLE_COUNT];
static volatile uint32_t benchmark_sink;
static uint64_t prng_state = 0x243f6a8885a308d3ULL ^ SECURITY_BITS;

static uint64_t fixed_random_u64(void)
{
  uint64_t z = (prng_state += 0x9e3779b97f4a7c15ULL);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

static void prepare_inputs(void)
{
  for (size_t sample = 0; sample < SAMPLE_COUNT; sample++) {
    uint64_t any = 0u;
    for (size_t word = 0; word < AIM3_NUM_WORDS_FIELD; word++) {
      inputs_a[sample][word] = fixed_random_u64();
      inputs_b[sample][word] = fixed_random_u64();
      any |= inputs_a[sample][word];
    }
    if (any == 0u) {
      inputs_a[sample][0] = 1u;
    }
  }
}

static uint32_t checksum_outputs(uint32_t state)
{
  for (size_t sample = 0; sample < SAMPLE_COUNT; sample++) {
    for (size_t word = 0; word < AIM3_NUM_WORDS_FIELD; word++) {
      uint64_t current = outputs[sample][word];
      for (size_t byte = 0; byte < 8u; byte++) {
        state ^= (uint8_t)current;
        state *= 16777619u;
        current >>= 8;
      }
    }
  }
  return state;
}

static void warm_up(void)
{
  gf temporary;
  for (size_t iteration = 0; iteration < WARMUP_COUNT; iteration++) {
    const size_t index = iteration % SAMPLE_COUNT;
    gf_mul(temporary, inputs_a[index], inputs_b[index]);
    gf_sqr(temporary, inputs_a[index]);
    gf_inv(temporary, inputs_a[index]);
    benchmark_sink ^= (uint32_t)temporary[0];
  }
}

static void measure_mul(void)
{
  for (size_t sample = 0; sample < SAMPLE_COUNT; sample++) {
    const uint32_t start = platform_cycle_begin();
    gf_mul(outputs[sample], inputs_a[sample], inputs_b[sample]);
    mul_cycles[sample] = platform_cycle_end(start);
  }
}

static void measure_sqr(void)
{
  for (size_t sample = 0; sample < SAMPLE_COUNT; sample++) {
    const uint32_t start = platform_cycle_begin();
    gf_sqr(outputs[sample], inputs_a[sample]);
    sqr_cycles[sample] = platform_cycle_end(start);
  }
}

static void measure_inv(void)
{
  for (size_t sample = 0; sample < SAMPLE_COUNT; sample++) {
    const uint32_t start = platform_cycle_begin();
    gf_inv(outputs[sample], inputs_a[sample]);
    inv_cycles[sample] = platform_cycle_end(start);
  }
}

static void emit_samples(const char *operation, const uint32_t *cycles)
{
  for (size_t sample = 0; sample < SAMPLE_COUNT; sample++) {
    platform_puts("SAMPLE,");
    platform_puts(operation);
    platform_putc(',');
    platform_put_u32((uint32_t)sample);
    platform_putc(',');
    platform_put_u32(cycles[sample]);
    platform_puts("\r\n");
  }
}

int main(void)
{
  platform_init();
  platform_wait_for_host();
  prepare_inputs();
  warm_up();

  uint32_t checksum = 2166136261u;
  measure_mul();
  checksum = checksum_outputs(checksum);
  measure_sqr();
  checksum = checksum_outputs(checksum);
  measure_inv();
  checksum = checksum_outputs(checksum);
  benchmark_sink ^= checksum;

  platform_puts("KERNEL_BENCH_BEGIN\r\nimplementation=" IMPL_NAME "\r\nfield_bits=");
  platform_put_u32(SECURITY_BITS);
  platform_puts("\r\nclock_hz=160000000\r\nwarmup_count=");
  platform_put_u32(WARMUP_COUNT);
  platform_puts("\r\nsamples_per_operation=");
  platform_put_u32(SAMPLE_COUNT);
  platform_puts("\r\n");
  emit_samples("gf_mul", mul_cycles);
  emit_samples("gf_sqr", sqr_cycles);
  emit_samples("gf_inv", inv_cycles);
  platform_puts("checksum=");
  platform_put_hex32(benchmark_sink);
  platform_puts("\r\nBENCH_RESULT=PASS\r\nDONE=pass\r\n");
  for (;;) {
    __asm volatile("wfi");
  }
}

void _exit(int code)
{
  platform_fatal_exit(code);
}
