#include "allocator.h"
#include "platform.h"

#include "aim3.h"
#include "api.h"
#include "common/fips202.h"
#include "common/rng.h"
#include "sign.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern unsigned char _heap_start[];
extern unsigned char _heap_limit[];
extern unsigned char _estack[];

#define FIXED_MESSAGE_BYTES 59u

static uint8_t signature[CRYPTO_BYTES] __attribute__((aligned(32)));
static uint8_t public_key[CRYPTO_PUBLICKEYBYTES] __attribute__((aligned(32)));
static uint8_t secret_key[CRYPTO_SECRETKEYBYTES] __attribute__((aligned(32)));
static uint8_t message[FIXED_MESSAGE_BYTES] __attribute__((aligned(32)));

static size_t align8(size_t value)
{
  return (value + 7u) & ~(size_t)7u;
}

static size_t allocation_cost(size_t payload)
{
  return align8(payload) + allocator_header_size();
}

static uint64_t signing_payload_requirement(void)
{
  const uint64_t nodes = (uint64_t)AIMER_T * (2u * AIMER_N - 1u) * AIMER_SEED_SIZE;
  const uint64_t commits = (uint64_t)AIMER_T * AIMER_N * AIMER_COMMIT_SIZE;
  const uint64_t checks = (uint64_t)AIMER_T * AIMER_N * sizeof(mult_chk_t);
  const uint64_t alpha = (uint64_t)AIMER_T * AIMER_N * (AIMER_L + 1u) * sizeof(gf);
  const uint64_t v = (uint64_t)AIMER_T * AIMER_N * sizeof(gf);
  return nodes + commits + checks + alpha + v + sizeof(aim_lin_t) +
         2u * PQC_SHAKEINCCTX_BYTES;
}

static uint64_t signing_arena_requirement(void)
{
  const size_t nodes = (size_t)AIMER_T * (2u * AIMER_N - 1u) * AIMER_SEED_SIZE;
  const size_t commits = (size_t)AIMER_T * AIMER_N * AIMER_COMMIT_SIZE;
  const size_t checks = (size_t)AIMER_T * AIMER_N * sizeof(mult_chk_t);
  const size_t alpha = (size_t)AIMER_T * AIMER_N * (AIMER_L + 1u) * sizeof(gf);
  const size_t v = (size_t)AIMER_T * AIMER_N * sizeof(gf);
  return allocation_cost(nodes) + allocation_cost(commits) +
         allocation_cost(checks) + allocation_cost(alpha) +
         allocation_cost(v) + allocation_cost(sizeof(aim_lin_t)) +
         2u * allocation_cost(PQC_SHAKEINCCTX_BYTES);
}

static uint32_t checksum32(const uint8_t *data, size_t length, uint32_t state)
{
  for (size_t i = 0; i < length; i++) {
    state ^= data[i];
    state *= 16777619u;
  }
  return state;
}

static void report_allocator(const char *operation)
{
  platform_puts("ALLOC,");
  platform_puts(operation);
  platform_puts(",peak_payload=");
  platform_put_u64(allocator_peak_payload());
  platform_puts(",peak_arena=");
  platform_put_u64(allocator_peak_arena());
  platform_puts(",failed_request=");
  platform_put_u64(allocator_failed_request());
  platform_puts(",live=");
  platform_put_u64(allocator_current_payload());
  platform_puts("\r\n");
}

int main(void)
{
  platform_init();
  platform_wait_for_host();

  platform_puts("PREFLIGHT_BEGIN\r\n");
  platform_puts("implementation=" IMPL_NAME "\r\n");
  platform_puts("parameter=");
  platform_puts(CRYPTO_ALGNAME);
  platform_puts("\r\n");
  platform_kv_u32("cpu_hz", 160000000u);
  platform_kv_hex32("cpuid", *(volatile uint32_t *)0xe000ed00u);
  platform_kv_hex32("dbgmcu_idcode", *(volatile uint32_t *)0xe0042000u);
  platform_kv_u32("flash_kib", *(volatile uint16_t *)0x1ff0f442u);
  platform_kv_hex32("scb_ccr", *(volatile uint32_t *)0xe000ed14u);
  platform_kv_hex32("flash_acr", *(volatile uint32_t *)0x40023c00u);
  platform_kv_hex32("rcc_cfgr", *(volatile uint32_t *)0x40023808u);
  platform_kv_u32("dwt_cyccnt_available", platform_dwt_available());
  platform_kv_u64("sram_bytes", 512u * 1024u);
  platform_kv_u64("stack_reserve_bytes", 32u * 1024u);
  platform_kv_u64("static_ram_bytes",
                  (uint64_t)((uintptr_t)_heap_start - 0x20000000u));
  platform_kv_u64("heap_capacity_bytes", allocator_capacity());
  platform_kv_u64("allocator_header_bytes", allocator_header_size());
  platform_kv_u64("sign_peak_payload_required", signing_payload_requirement());
  platform_kv_u64("sign_peak_arena_required", signing_arena_requirement());
  platform_kv_u64("sign_total_sram_required",
                  (uint64_t)((uintptr_t)_heap_start - 0x20000000u) +
                  32u * 1024u + signing_arena_requirement());
  platform_kv_hex32("heap_start", (uint32_t)(uintptr_t)_heap_start);
  platform_kv_hex32("heap_limit", (uint32_t)(uintptr_t)_heap_limit);
  platform_kv_hex32("stack_top", (uint32_t)(uintptr_t)_estack);

  uint8_t entropy[48];
  for (size_t i = 0; i < sizeof(entropy); i++) {
    entropy[i] = (uint8_t)i;
  }
  for (size_t i = 0; i < sizeof(message); i++) {
    message[i] = (uint8_t)(0xa5u ^ (uint8_t)i);
  }
  randombytes_init(entropy, NULL, 256);

  int failed = 0;
  allocator_reset_stats();
  const int keypair_result = crypto_sign_keypair(public_key, secret_key);
  report_allocator("keypair");
  platform_kv_u32("keypair_result", (uint32_t)keypair_result);
  if (keypair_result != 0) {
    failed = 1;
  }

  size_t signature_length = 0;
  int sign_result = -999;
  int verify_result = -999;
  int tamper_result = -999;
  if (!failed) {
    allocator_reset_stats();
    sign_result = crypto_sign_signature(signature, &signature_length,
                                        message, sizeof(message), NULL, 0,
                                        secret_key);
    report_allocator("sign");
    platform_kv_u32("sign_result", (uint32_t)sign_result);
    platform_kv_u64("signature_length", signature_length);
    if (sign_result != 0 || signature_length != CRYPTO_BYTES) {
      failed = 1;
    }
  }

  if (!failed) {
    allocator_reset_stats();
    verify_result = crypto_sign_verify(signature, signature_length,
                                       message, sizeof(message), NULL, 0,
                                       public_key);
    report_allocator("verify");
    platform_kv_u32("verify_result", (uint32_t)verify_result);
    if (verify_result != 0) {
      failed = 1;
    }
  }

  if (!failed) {
    signature[signature_length / 2u] ^= 1u;
    allocator_reset_stats();
    tamper_result = crypto_sign_verify(signature, signature_length,
                                       message, sizeof(message), NULL, 0,
                                       public_key);
    report_allocator("tamper_verify");
    platform_kv_u32("tamper_result", (uint32_t)tamper_result);
    signature[signature_length / 2u] ^= 1u;
    if (tamper_result == 0) {
      failed = 1;
    }
  }

  uint32_t checksum = 2166136261u;
  checksum = checksum32(public_key, sizeof(public_key), checksum);
  checksum = checksum32(secret_key, sizeof(secret_key), checksum);
  if (sign_result == 0) {
    checksum = checksum32(signature, signature_length, checksum);
  }
  platform_kv_hex32("checksum", checksum);
  platform_puts(failed ? "PREFLIGHT_RESULT=FAIL\r\n" :
                         "PREFLIGHT_RESULT=PASS\r\n");
  platform_puts(failed ? "DONE=fail\r\n" : "DONE=pass\r\n");

  for (;;) {
    __asm volatile("wfi");
  }
}

void _exit(int code)
{
  platform_fatal_exit(code);
}
