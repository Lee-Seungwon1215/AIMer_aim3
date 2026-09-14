#ifndef AIMER_M7_ALLOCATOR_H
#define AIMER_M7_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

void allocator_reset_stats(void);
size_t allocator_current_payload(void);
size_t allocator_peak_payload(void);
size_t allocator_peak_arena(void);
size_t allocator_capacity(void);
size_t allocator_failed_request(void);
size_t allocator_header_size(void);

#endif
