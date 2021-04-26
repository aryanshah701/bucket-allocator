#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <math.h>

#include "xmalloc.h"
#include "hmalloc.h"

#ifndef BITMAP_H
#define BITMAP_H

typedef struct bitmap {
  int num_bits;
  size_t arr_size;
  int32_t* arr;
} bitmap;

void change_bit(bitmap bb, int32_t kth_block, int32_t val);
long find_free_bit(bitmap bb);
long is_empty_bitmap(bitmap bb);

// void change_bit(bitmap* bb, int32_t kth_block, int32_t val)
// long find_free_bit(bitmap* bb)

#endif