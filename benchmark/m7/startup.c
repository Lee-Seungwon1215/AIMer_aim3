#include <stdint.h>

extern unsigned char _sidata;
extern unsigned char _sdata;
extern unsigned char _edata;
extern unsigned char _sbss;
extern unsigned char _ebss;
extern unsigned char _estack;

int main(void);

void Reset_Handler(void);
static void Default_Handler(void);

__attribute__((section(".isr_vector"), used))
void (*const vectors[])(void) = {
  (void (*)(void))(&_estack),
  Reset_Handler,
  Default_Handler,
  Default_Handler,
  Default_Handler,
  Default_Handler,
  Default_Handler,
  0,
  0,
  0,
  0,
  Default_Handler,
  Default_Handler,
  0,
  Default_Handler,
  Default_Handler,
};

void Reset_Handler(void)
{
  unsigned char *source = &_sidata;
  for (unsigned char *dest = &_sdata; dest < &_edata;) {
    *dest++ = *source++;
  }
  for (unsigned char *dest = &_sbss; dest < &_ebss;) {
    *dest++ = 0;
  }
  (void)main();
  for (;;) {
  }
}

static void Default_Handler(void)
{
  for (;;) {
  }
}
