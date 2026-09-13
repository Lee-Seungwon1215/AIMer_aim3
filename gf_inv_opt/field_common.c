// SPDX-License-Identifier: MIT

#include "field.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void gf_to_bytes(uint8_t *out, const gf in)
{
  for (size_t i = 0; i < AIM3_NUM_WORDS_FIELD; i++)
  {
    uint64_t u = in[i];
    for (size_t j = 0; j < 8; j++)
    {
      out[8 * i + j] = (uint8_t)u;
      u >>= 8;
    }
  }
}

void gf_from_bytes(gf out, const uint8_t *in)
{
  for (size_t i = 0; i < AIM3_NUM_WORDS_FIELD; i++)
  {
    uint64_t u = 0;
    for (int j = 7; j >= 0; j--)
    {
      u = (u << 8) | in[8 * i + j];
    }
    out[i] = u;
  }
}

void gf_set0(gf a)
{
  for (size_t i = 0; i < AIM3_NUM_WORDS_FIELD; i++)
  {
    a[i] = 0;
  }
}

bool gf_is0(const gf a)
{
  uint64_t result = 0;
  for (size_t i = 0; i < AIM3_NUM_WORDS_FIELD; i++)
  {
    result |= a[i];
  }
  return result == 0;
}

void gf_copy(gf out, const gf in)
{
  for (size_t i = 0; i < AIM3_NUM_WORDS_FIELD; i++)
  {
    out[i] = in[i];
  }
}

void gf_add(gf c, const gf a, const gf b)
{
  for (size_t i = 0; i < AIM3_NUM_WORDS_FIELD; i++)
  {
    c[i] = a[i] ^ b[i];
  }
}

static void gf_sqr_n_mul(gf out, const gf x, size_t n, const gf y)
{
  gf squared;
  gf product;

  gf_copy(squared, x);
  for (size_t i = 0; i < n; i++)
  {
    gf_sqr(squared, squared);
  }

  gf_mul(product, squared, y);
  gf_copy(out, product);
}

void gf_inv(gf c, const gf a)
{
#if SECURITY_BITS == 128
  gf a3;
  gf a7;
  gf t;

  gf_sqr_n_mul(t, a, 1, a);      // A_2
  gf_sqr_n_mul(a3, t, 1, a);     // A_3
  gf_sqr_n_mul(t, a3, 3, a3);    // A_6
  gf_sqr_n_mul(a7, t, 1, a);     // A_7
  gf_sqr_n_mul(t, a7, 7, a7);    // A_14
  gf_sqr_n_mul(t, t, 1, a);      // A_15
  gf_sqr_n_mul(t, t, 15, t);     // A_30
  gf_sqr_n_mul(t, t, 30, t);     // A_60
  gf_sqr_n_mul(t, t, 60, t);     // A_120
  gf_sqr_n_mul(t, t, 7, a7);     // A_127
#elif SECURITY_BITS == 192
  gf a2;
  gf a3;
  gf t;

  gf_sqr_n_mul(a2, a, 1, a);     // A_2
  gf_sqr_n_mul(a3, a2, 1, a);    // A_3
  gf_sqr_n_mul(t, a3, 2, a2);    // A_5
  gf_sqr_n_mul(t, t, 5, t);      // A_10
  gf_sqr_n_mul(t, t, 10, t);     // A_20
  gf_sqr_n_mul(t, t, 3, a3);     // A_23
  gf_sqr_n_mul(t, t, 23, t);     // A_46
  gf_sqr_n_mul(t, t, 1, a);      // A_47
  gf_sqr_n_mul(t, t, 47, t);     // A_94
  gf_sqr_n_mul(t, t, 94, t);     // A_188
  gf_sqr_n_mul(t, t, 3, a3);     // A_191
#elif SECURITY_BITS == 256
  gf a2;
  gf a3;
  gf t;

  gf_sqr_n_mul(a2, a, 1, a);     // A_2
  gf_sqr_n_mul(a3, a2, 1, a);    // A_3
  gf_sqr_n_mul(a2, a3, 2, a2);   // A_5
  gf_sqr_n_mul(t, a2, 5, a2);    // A_10
  gf_sqr_n_mul(a3, t, 5, a2);    // A_15
  gf_sqr_n_mul(t, a3, 15, a3);   // A_30
  gf_sqr_n_mul(t, t, 30, t);     // A_60
  gf_sqr_n_mul(t, t, 60, t);     // A_120
  gf_sqr_n_mul(t, t, 120, t);    // A_240
  gf_sqr_n_mul(t, t, 15, a3);    // A_255
#else
#error "Unsupported AIMer field size"
#endif

  gf_sqr(c, t);
}

// c += a * b
void gf_mul_add(gf c, const gf a, const gf b)
{
  gf r;
  gf_mul(r, a, b);
  gf_add(c, c, r);
}

// c += sum_i a[i] * b[i]
void gf_mat_vec_mul_add(gf c, const gf a, const gf b[AIM3_NUM_BITS_FIELD])
{
  gf r;
  gf_mat_vec_mul(r, a, b);
  gf_add(c, c, r);
}
