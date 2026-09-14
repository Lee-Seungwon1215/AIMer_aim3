#ifndef AIMER_M7_PLATFORM_H
#define AIMER_M7_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

void platform_init(void);
void platform_wait_for_host(void);
void platform_putc(char c);
void platform_puts(const char *s);
void platform_put_u32(uint32_t value);
void platform_put_u64(uint64_t value);
void platform_put_hex32(uint32_t value);
void platform_kv_u32(const char *key, uint32_t value);
void platform_kv_u64(const char *key, uint64_t value);
void platform_kv_hex32(const char *key, uint32_t value);
uint32_t platform_dwt_available(void);
void platform_fatal_exit(int code) __attribute__((noreturn));

static inline uint32_t platform_cycle_begin(void)
{
  __asm volatile("dsb 0xf\n\tisb 0xf" ::: "memory");
  return *(volatile uint32_t *)(uintptr_t)0xe0001004u;
}

static inline uint32_t platform_cycle_end(uint32_t start)
{
  __asm volatile("dsb 0xf\n\tisb 0xf" ::: "memory");
  const uint32_t end = *(volatile uint32_t *)(uintptr_t)0xe0001004u;
  return end - start;
}

#endif
