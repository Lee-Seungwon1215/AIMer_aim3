#include "platform.h"

#include "field.h"

#include <stddef.h>
#include <stdint.h>

#define DIFFERENTIAL_INPUTS 4096u

void m7_opt_gf_inv(gf out, const gf in);

static uint64_t prng_state = 0x6a09e667f3bcc909ULL ^ SECURITY_BITS;

static uint64_t fixed_random_u64(void)
{
  uint64_t z = (prng_state += 0x9e3779b97f4a7c15ULL);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

static void fixed_nonzero_input(gf out)
{
  uint64_t any = 0u;
  for (size_t word = 0; word < AIM3_NUM_WORDS_FIELD; word++) {
    out[word] = fixed_random_u64();
    any |= out[word];
  }
  if (any == 0u) {
    out[0] = 1u;
  }
}

static int field_equal(const gf left, const gf right)
{
  uint64_t difference = 0u;
  for (size_t word = 0; word < AIM3_NUM_WORDS_FIELD; word++) {
    difference |= left[word] ^ right[word];
  }
  return difference == 0u;
}

static int field_is_one(const gf value)
{
  uint64_t difference = value[0] ^ 1u;
  for (size_t word = 1; word < AIM3_NUM_WORDS_FIELD; word++) {
    difference |= value[word];
  }
  return difference == 0u;
}

static uint32_t checksum_field(uint32_t state, const gf value)
{
  for (size_t word = 0; word < AIM3_NUM_WORDS_FIELD; word++) {
    uint64_t current = value[word];
    for (size_t byte = 0; byte < 8u; byte++) {
      state ^= (uint8_t)current;
      state *= 16777619u;
      current >>= 8;
    }
  }
  return state;
}

static int fail_at(const char *check, uint32_t index)
{
  platform_puts("FAIL_CHECK=");
  platform_puts(check);
  platform_puts("\r\nFAIL_INDEX=");
  platform_put_u32(index);
  platform_puts("\r\nCORRECTNESS_RESULT=FAIL\r\nDONE=fail\r\n");
  return 1;
}

int main(void)
{
  platform_init();
  platform_wait_for_host();

  platform_puts("FIELD_CORRECTNESS_BEGIN\r\nfield_bits=");
  platform_put_u32(SECURITY_BITS);
  platform_puts("\r\nfixed_nonzero_inputs=");
  platform_put_u32(DIFFERENTIAL_INPUTS);
  platform_puts("\r\n");

  uint32_t checksum = 2166136261u;
  for (uint32_t index = 0; index < DIFFERENTIAL_INPUTS; index++) {
    gf input;
    gf reference_inverse;
    gf optimized_inverse;
    gf reference_identity;
    gf optimized_identity;
    gf reference_in_place;
    gf optimized_in_place;

    fixed_nonzero_input(input);
    gf_inv(reference_inverse, input);
    m7_opt_gf_inv(optimized_inverse, input);
    if (!field_equal(reference_inverse, optimized_inverse)) {
      return fail_at("reference_optimized_differential", index);
    }

    gf_mul(reference_identity, input, reference_inverse);
    if (!field_is_one(reference_identity)) {
      return fail_at("reference_inverse_identity", index);
    }
    gf_mul(optimized_identity, input, optimized_inverse);
    if (!field_is_one(optimized_identity)) {
      return fail_at("optimized_inverse_identity", index);
    }

    gf_copy(reference_in_place, input);
    gf_inv(reference_in_place, reference_in_place);
    if (!field_equal(reference_in_place, reference_inverse)) {
      return fail_at("reference_in_place", index);
    }
    gf_copy(optimized_in_place, input);
    m7_opt_gf_inv(optimized_in_place, optimized_in_place);
    if (!field_equal(optimized_in_place, optimized_inverse)) {
      return fail_at("optimized_in_place", index);
    }

    checksum = checksum_field(checksum, input);
    checksum = checksum_field(checksum, reference_inverse);
    checksum = checksum_field(checksum, optimized_inverse);
  }

  gf zero;
  gf reference_zero;
  gf optimized_zero;
  gf_set0(zero);
  gf_inv(reference_zero, zero);
  m7_opt_gf_inv(optimized_zero, zero);
  if (!field_equal(reference_zero, optimized_zero)) {
    return fail_at("zero_differential", DIFFERENTIAL_INPUTS);
  }
  if (!gf_is0(reference_zero) || !gf_is0(optimized_zero)) {
    return fail_at("zero_result", DIFFERENTIAL_INPUTS);
  }

  checksum = checksum_field(checksum, reference_zero);
  checksum = checksum_field(checksum, optimized_zero);
  platform_puts("differential=PASS\r\nidentity=PASS\r\nin_place=PASS\r\nzero=PASS\r\nchecksum=");
  platform_put_hex32(checksum);
  platform_puts("\r\nCORRECTNESS_RESULT=PASS\r\nDONE=pass\r\n");
  for (;;) {
    __asm volatile("wfi");
  }
}

void _exit(int code)
{
  platform_fatal_exit(code);
}
