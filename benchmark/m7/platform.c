#include "platform.h"

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define RCC_BASE       0x40023800u
#define RCC_CR         REG32(RCC_BASE + 0x00u)
#define RCC_PLLCFGR    REG32(RCC_BASE + 0x04u)
#define RCC_CFGR       REG32(RCC_BASE + 0x08u)
#define RCC_AHB1ENR    REG32(RCC_BASE + 0x30u)
#define RCC_APB1ENR    REG32(RCC_BASE + 0x40u)

#define FLASH_ACR      REG32(0x40023c00u)
#define GPIOD_MODER    REG32(0x40020c00u)
#define GPIOD_OSPEEDR  REG32(0x40020c08u)
#define GPIOD_PUPDR    REG32(0x40020c0cu)
#define GPIOD_AFRH     REG32(0x40020c24u)

#define USART3_BASE    0x40004800u
#define USART3_CR1     REG32(USART3_BASE + 0x00u)
#define USART3_CR2     REG32(USART3_BASE + 0x04u)
#define USART3_CR3     REG32(USART3_BASE + 0x08u)
#define USART3_BRR     REG32(USART3_BASE + 0x0cu)
#define USART3_ISR     REG32(USART3_BASE + 0x1cu)
#define USART3_RDR     REG32(USART3_BASE + 0x24u)
#define USART3_TDR     REG32(USART3_BASE + 0x28u)

#define SCB_CCR        REG32(0xe000ed14u)
#define SCB_CSSELR     REG32(0xe000ed84u)
#define SCB_CCSIDR     REG32(0xe000ed80u)
#define SCB_ICIALLU    REG32(0xe000ef50u)
#define SCB_DCISW      REG32(0xe000ef60u)
#define COREDEBUG_DEMCR REG32(0xe000edfcu)
#define DWT_CTRL       REG32(0xe0001000u)
#define DWT_CYCCNT     REG32(0xe0001004u)
#define DWT_LAR        REG32(0xe0001fb0u)

static inline void dsb(void)
{
  __asm volatile("dsb 0xf" ::: "memory");
}

static inline void isb(void)
{
  __asm volatile("isb 0xf" ::: "memory");
}

static void clock_init_160mhz(void)
{
  /* HSI=16 MHz, PLLM=8, PLLN=160, PLLP=2: SYSCLK/HCLK=160 MHz. */
  RCC_CR |= 1u;
  while ((RCC_CR & (1u << 1)) == 0u) {
  }

  RCC_CFGR &= ~3u;
  while ((RCC_CFGR & (3u << 2)) != 0u) {
  }

  RCC_CR &= ~(1u << 24);
  while ((RCC_CR & (1u << 25)) != 0u) {
  }

  FLASH_ACR = (7u << 0) | (1u << 8) | (1u << 9);
  RCC_PLLCFGR = 8u | (160u << 6) | (0u << 16) | (8u << 24);
  RCC_CFGR = (RCC_CFGR & ~((15u << 4) | (7u << 10) | (7u << 13))) |
             (5u << 10) | (4u << 13);
  RCC_CR |= 1u << 24;
  while ((RCC_CR & (1u << 25)) == 0u) {
  }
  RCC_CFGR = (RCC_CFGR & ~3u) | 2u;
  while ((RCC_CFGR & (3u << 2)) != (2u << 2)) {
  }
}

static void cache_enable(void)
{
  if ((SCB_CCR & (1u << 16)) == 0u) {
    SCB_CSSELR = 0u;
    dsb();
    const uint32_t ccsidr = SCB_CCSIDR;
    uint32_t sets = (ccsidr >> 13) & 0x7fffu;
    const uint32_t ways = (ccsidr >> 3) & 0x3ffu;
    do {
      uint32_t way = ways;
      do {
        SCB_DCISW = (sets << 5) | (way << 30);
      } while (way-- != 0u);
    } while (sets-- != 0u);
    dsb();
    SCB_CCR |= 1u << 16;
    dsb();
    isb();
  }

  if ((SCB_CCR & (1u << 17)) == 0u) {
    SCB_ICIALLU = 0u;
    dsb();
    isb();
    SCB_CCR |= 1u << 17;
    dsb();
    isb();
  }
}

static void uart_init(void)
{
  RCC_AHB1ENR |= 1u << 3;
  RCC_APB1ENR |= 1u << 18;
  (void)RCC_AHB1ENR;
  (void)RCC_APB1ENR;

  GPIOD_MODER = (GPIOD_MODER & ~((3u << 16) | (3u << 18))) |
                (2u << 16) | (2u << 18);
  GPIOD_OSPEEDR |= (3u << 16) | (3u << 18);
  GPIOD_PUPDR = (GPIOD_PUPDR & ~((3u << 16) | (3u << 18))) |
                (1u << 18);
  GPIOD_AFRH = (GPIOD_AFRH & ~0xffu) | 0x77u;

  USART3_CR1 = 0u;
  USART3_CR2 = 0u;
  USART3_CR3 = 0u;
  USART3_BRR = 347u; /* PCLK1=40 MHz, 115200 baud, oversampling by 16. */
  USART3_CR1 = (1u << 0) | (1u << 3) | (1u << 2);
}

static void dwt_init(void)
{
  COREDEBUG_DEMCR |= 1u << 24;
  DWT_LAR = 0xc5acce55u;
  DWT_CYCCNT = 0u;
  DWT_CTRL |= 1u;
  dsb();
  isb();
}

void platform_init(void)
{
  __asm volatile("cpsid i" ::: "memory");
  REG32(0xe000ed88u) |= 0x00f00000u;
  dsb();
  isb();
  clock_init_160mhz();
  cache_enable();
  uart_init();
  dwt_init();
}

void platform_wait_for_host(void)
{
  platform_puts("READY\r\n");
  while ((USART3_ISR & (1u << 5)) == 0u) {
  }
  (void)USART3_RDR;
}

void platform_putc(char c)
{
  while ((USART3_ISR & (1u << 7)) == 0u) {
  }
  USART3_TDR = (uint32_t)(uint8_t)c;
}

void platform_puts(const char *s)
{
  while (*s != '\0') {
    platform_putc(*s++);
  }
}

void platform_put_u32(uint32_t value)
{
  char digits[10];
  size_t used = 0;
  do {
    digits[used++] = (char)('0' + value % 10u);
    value /= 10u;
  } while (value != 0u);
  while (used != 0u) {
    platform_putc(digits[--used]);
  }
}

void platform_put_u64(uint64_t value)
{
  char digits[20];
  size_t used = 0;
  do {
    digits[used++] = (char)('0' + value % 10u);
    value /= 10u;
  } while (value != 0u);
  while (used != 0u) {
    platform_putc(digits[--used]);
  }
}

void platform_put_hex32(uint32_t value)
{
  static const char hex[] = "0123456789abcdef";
  platform_puts("0x");
  for (int shift = 28; shift >= 0; shift -= 4) {
    platform_putc(hex[(value >> (unsigned)shift) & 0xfu]);
  }
}

void platform_kv_u32(const char *key, uint32_t value)
{
  platform_puts(key);
  platform_putc('=');
  platform_put_u32(value);
  platform_puts("\r\n");
}

void platform_kv_u64(const char *key, uint64_t value)
{
  platform_puts(key);
  platform_putc('=');
  platform_put_u64(value);
  platform_puts("\r\n");
}

void platform_kv_hex32(const char *key, uint32_t value)
{
  platform_puts(key);
  platform_putc('=');
  platform_put_hex32(value);
  platform_puts("\r\n");
}

uint32_t platform_dwt_available(void)
{
  const uint32_t before = DWT_CYCCNT;
  for (volatile uint32_t i = 0; i < 64u; i++) {
    __asm volatile("nop");
  }
  return DWT_CYCCNT != before;
}

void platform_fatal_exit(int code)
{
  platform_puts("FATAL_EXIT=");
  platform_put_u32((uint32_t)code);
  platform_puts("\r\nDONE=fatal\r\n");
  for (;;) {
    __asm volatile("wfi");
  }
}
