#include "platform.h"

#include "api.h"
#include "common/rng.h"

#include <stddef.h>
#include <stdint.h>

#define SAMPLE_COUNT 50u
#define WARMUP_COUNT 3u
#define FIXED_MESSAGE_BYTES 59u

#ifndef E2E_SIGN_VERIFY
#define E2E_SIGN_VERIFY 0
#endif

static uint8_t signature[CRYPTO_BYTES] __attribute__((aligned(32)));
static uint8_t public_key[CRYPTO_PUBLICKEYBYTES] __attribute__((aligned(32)));
static uint8_t secret_key[CRYPTO_SECRETKEYBYTES] __attribute__((aligned(32)));
static uint8_t message[FIXED_MESSAGE_BYTES] __attribute__((aligned(32)));
static uint32_t keypair_cycles[SAMPLE_COUNT];
#if E2E_SIGN_VERIFY
static uint32_t sign_cycles[SAMPLE_COUNT];
static uint32_t verify_cycles[SAMPLE_COUNT];
#endif
static volatile uint32_t benchmark_sink;

static uint32_t checksum_bytes(uint32_t state, const uint8_t *data, size_t length)
{
  for (size_t index = 0; index < length; index++) {
    state ^= data[index];
    state *= 16777619u;
  }
  return state;
}

static void initialize_fixed_data(void)
{
  uint8_t entropy[48];
  for (size_t index = 0; index < sizeof(entropy); index++) {
    entropy[index] = (uint8_t)index;
  }
  for (size_t index = 0; index < sizeof(message); index++) {
    message[index] = (uint8_t)(0xa5u ^ (uint8_t)index);
  }
  randombytes_init(entropy, NULL, 256);
}

static int benchmark_keypair(uint32_t *checksum)
{
  for (size_t iteration = 0; iteration < WARMUP_COUNT; iteration++) {
    if (crypto_sign_keypair(public_key, secret_key) != 0) {
      return -1;
    }
    benchmark_sink ^= public_key[iteration % sizeof(public_key)];
  }

  for (size_t sample = 0; sample < SAMPLE_COUNT; sample++) {
    const uint32_t start = platform_cycle_begin();
    const int result = crypto_sign_keypair(public_key, secret_key);
    keypair_cycles[sample] = platform_cycle_end(start);
    if (result != 0) {
      return -1;
    }
    *checksum = checksum_bytes(*checksum, public_key, sizeof(public_key));
    *checksum = checksum_bytes(*checksum, secret_key, sizeof(secret_key));
  }
  return 0;
}

#if E2E_SIGN_VERIFY
static int benchmark_sign(uint32_t *checksum, size_t *last_signature_length)
{
  for (size_t iteration = 0; iteration < WARMUP_COUNT; iteration++) {
    size_t signature_length = 0u;
    if (crypto_sign_signature(signature, &signature_length,
                              message, sizeof(message), NULL, 0,
                              secret_key) != 0 ||
        signature_length != CRYPTO_BYTES) {
      return -1;
    }
    benchmark_sink ^= signature[iteration % signature_length];
  }

  for (size_t sample = 0; sample < SAMPLE_COUNT; sample++) {
    size_t signature_length = 0u;
    const uint32_t start = platform_cycle_begin();
    const int result = crypto_sign_signature(signature, &signature_length,
                                             message, sizeof(message),
                                             NULL, 0, secret_key);
    sign_cycles[sample] = platform_cycle_end(start);
    if (result != 0 || signature_length != CRYPTO_BYTES) {
      return -1;
    }
    *checksum = checksum_bytes(*checksum, signature, signature_length);
    *last_signature_length = signature_length;
  }
  return 0;
}

static int benchmark_verify(uint32_t *checksum, size_t signature_length)
{
  for (size_t iteration = 0; iteration < WARMUP_COUNT; iteration++) {
    if (crypto_sign_verify(signature, signature_length,
                           message, sizeof(message), NULL, 0,
                           public_key) != 0) {
      return -1;
    }
    benchmark_sink ^= (uint32_t)iteration;
  }

  for (size_t sample = 0; sample < SAMPLE_COUNT; sample++) {
    const uint32_t start = platform_cycle_begin();
    const int result = crypto_sign_verify(signature, signature_length,
                                          message, sizeof(message), NULL, 0,
                                          public_key);
    verify_cycles[sample] = platform_cycle_end(start);
    if (result != 0) {
      return -1;
    }
    *checksum = checksum_bytes(*checksum, signature, signature_length);
  }
  return 0;
}
#endif

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

static int fail(const char *operation)
{
  platform_puts("FAIL_OPERATION=");
  platform_puts(operation);
  platform_puts("\r\nBENCH_RESULT=FAIL\r\nDONE=fail\r\n");
  return 1;
}

int main(void)
{
  platform_init();
  platform_wait_for_host();
  initialize_fixed_data();

  uint32_t checksum = 2166136261u;
  if (benchmark_keypair(&checksum) != 0) {
    return fail("keypair");
  }

#if E2E_SIGN_VERIFY
  size_t signature_length = 0u;
  if (benchmark_sign(&checksum, &signature_length) != 0) {
    return fail("sign");
  }
  if (benchmark_verify(&checksum, signature_length) != 0) {
    return fail("verify");
  }
#endif

  benchmark_sink ^= checksum;
  platform_puts("E2E_BENCH_BEGIN\r\nimplementation=" IMPL_NAME "\r\nparameter=" CRYPTO_ALGNAME "\r\nclock_hz=160000000\r\nwarmup_count=");
  platform_put_u32(WARMUP_COUNT);
  platform_puts("\r\nsamples_per_operation=");
  platform_put_u32(SAMPLE_COUNT);
  platform_puts("\r\n");
  emit_samples("keypair", keypair_cycles);
#if E2E_SIGN_VERIFY
  emit_samples("sign", sign_cycles);
  emit_samples("verify", verify_cycles);
#else
  platform_puts("sign=N/A_insufficient_SRAM\r\nverify=N/A_insufficient_SRAM\r\n");
#endif
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
