#include "allocator.h"

#include <stdint.h>
#include <string.h>

extern unsigned char _heap_start[];
extern unsigned char _heap_limit[];

typedef struct block_header {
  uint32_t size;
  uint32_t requested;
  struct block_header *next;
  struct block_header *prev;
} block_header;

#define BLOCK_FREE 1u
#define BLOCK_SIZE_MASK (~(uint32_t)BLOCK_FREE)

static block_header *first_block;
static size_t current_payload;
static size_t peak_payload;
static size_t peak_arena;
static size_t failed_request;

static size_t align8(size_t value)
{
  return (value + 7u) & ~(size_t)7u;
}

static void allocator_init(void)
{
  if (first_block != NULL) {
    return;
  }
  first_block = (block_header *)(void *)_heap_start;
  const size_t capacity = (size_t)(_heap_limit - _heap_start);
  first_block->size = (uint32_t)(capacity - sizeof(block_header)) | BLOCK_FREE;
  first_block->requested = 0u;
  first_block->next = NULL;
  first_block->prev = NULL;
}

void *malloc(size_t requested)
{
  allocator_init();
  if (requested == 0u) {
    requested = 1u;
  }
  if (requested > UINT32_MAX - 7u) {
    failed_request = requested;
    return NULL;
  }

  const size_t aligned = align8(requested);
  for (block_header *block = first_block; block != NULL; block = block->next) {
    const size_t block_size = block->size & BLOCK_SIZE_MASK;
    if ((block->size & BLOCK_FREE) == 0u || block_size < aligned) {
      continue;
    }

    const size_t remaining = block_size - aligned;
    if (remaining >= sizeof(block_header) + 8u) {
      block_header *split = (block_header *)((unsigned char *)(block + 1) + aligned);
      split->size = (uint32_t)(remaining - sizeof(block_header)) | BLOCK_FREE;
      split->requested = 0u;
      split->next = block->next;
      split->prev = block;
      if (split->next != NULL) {
        split->next->prev = split;
      }
      block->next = split;
      block->size = (uint32_t)aligned;
    } else {
      block->size = (uint32_t)block_size;
    }
    block->requested = (uint32_t)requested;

    current_payload += requested;
    if (current_payload > peak_payload) {
      peak_payload = current_payload;
    }
    const size_t arena = (size_t)((unsigned char *)(block + 1) +
                                  (block->size & BLOCK_SIZE_MASK) -
                                  _heap_start);
    if (arena > peak_arena) {
      peak_arena = arena;
    }
    return block + 1;
  }

  failed_request = requested;
  return NULL;
}

void free(void *pointer)
{
  if (pointer == NULL) {
    return;
  }
  block_header *block = (block_header *)pointer - 1;
  current_payload -= block->requested;
  block->requested = 0u;
  block->size |= BLOCK_FREE;

  if (block->next != NULL && (block->next->size & BLOCK_FREE) != 0u) {
    block_header *next = block->next;
    block->size = (uint32_t)((block->size & BLOCK_SIZE_MASK) +
                             sizeof(block_header) +
                             (next->size & BLOCK_SIZE_MASK)) | BLOCK_FREE;
    block->next = next->next;
    if (block->next != NULL) {
      block->next->prev = block;
    }
  }
  if (block->prev != NULL && (block->prev->size & BLOCK_FREE) != 0u) {
    block_header *prev = block->prev;
    prev->size = (uint32_t)((prev->size & BLOCK_SIZE_MASK) +
                            sizeof(block_header) +
                            (block->size & BLOCK_SIZE_MASK)) | BLOCK_FREE;
    prev->next = block->next;
    if (prev->next != NULL) {
      prev->next->prev = prev;
    }
  }
}

void *calloc(size_t count, size_t size)
{
  if (size != 0u && count > SIZE_MAX / size) {
    failed_request = SIZE_MAX;
    return NULL;
  }
  const size_t total = count * size;
  void *pointer = malloc(total);
  if (pointer != NULL) {
    memset(pointer, 0, total);
  }
  return pointer;
}

void allocator_reset_stats(void)
{
  peak_payload = current_payload;
  peak_arena = 0u;
  failed_request = 0u;
}

size_t allocator_current_payload(void) { return current_payload; }
size_t allocator_peak_payload(void) { return peak_payload; }
size_t allocator_peak_arena(void) { return peak_arena; }
size_t allocator_capacity(void)
{
  return (size_t)(_heap_limit - _heap_start);
}
size_t allocator_failed_request(void) { return failed_request; }
size_t allocator_header_size(void) { return sizeof(block_header); }
