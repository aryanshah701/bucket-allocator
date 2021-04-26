#include "bitmap.h"
#include <math.h>
#include <stdio.h>


// sets a bit at index k equal to 0.
// It leaves all other bits as they were.
void
clear_bit(int32_t* arr, int32_t kth_block) {
  int32_t ii = kth_block / 32;
  int32_t pos = kth_block % 32;

  int32_t flag = 1;
  flag = flag << pos;
  // set flag from ...01000... to ...10111...
  flag = ~flag;
  arr[ii] = arr[ii] & flag;
}


void
print_val(int32_t* arr, int32_t kth_block) {
  int32_t ii = kth_block / 32;
  int32_t pos = kth_block % 32;

  int32_t flag = 1;
  // set flag to ...01000... 
  flag = flag << pos;
  uint32_t val = arr[ii] & flag;
  if (val == 0) {
    // printf("value is 0: %u\n", val);
  }
  else {
    // printf("value is 1: %u\n", val);
  }
}


// sets a bit at index k equal to 1.
// It leaves all other bits as they were.
void
set_bit(int32_t* arr, int32_t kth_block) {
  int32_t ii = kth_block / 32;
  int32_t pos = kth_block % 32;

  // set flag to ...00001
  int32_t flag = 1;
  flag = flag << pos;
  arr[ii] = arr[ii] | flag;

}

// Determines if the bit at index k must be set to 1 or 0.
void
change_bit(bitmap bb, int32_t kth_block, int32_t val) {
  int32_t* arr = bb.arr;
  if (val == 0) {
    clear_bit(arr, kth_block);
  }
  else {
    set_bit(arr, kth_block);
  }
}

// finds the first 0 in the array of ints. Returns -1 when there aren't any 0s in the int
// or the index (starting from 0 [so you will always get a value betwee 0 and 31]) 
long
find_first_zero(int32_t bit_array) {
  // if the unsigned num is maxed out, this should make it 0
  if (bit_array == UINT32_MAX) {
    return -1;
  }
  else {

    uint32_t res2 = ~bit_array & (bit_array + 1);
    long val2 = (long) res2;
    long op2 = log((double)val2)/log(2.0);

    return op2;
  }
}

// goes through the list of ints looking for the first 0.
// returns 0 if all the blocks have been allocated
long
find_free_bit(bitmap bb) {
  int32_t* arr = bb.arr;
  int nb = bb.num_bits;
  int size = (int) ceil(((double)nb) / 32.0);

  for (long ii = 0; ii < size; ++ii) {
    long val = find_first_zero(arr[ii]);
    // printf("index: %ld\n", val);
    if (val >= 0) {
      return val + 32 * ii;
    }
  }
  // if you found nothing, return -1
  return -1;
}

// Note: This function is here for testing.
// sets everything before the change_index = 1 and leaves change_index = 0
// ex. set_val(bb, 31) would leave the 31st bit (starting from index 0) at 0 and 
// make everything else 1.
void
set_val(bitmap* bb, int change_index) {
  int32_t* arr = bb->arr;
  int target_size = (int) floor(((double)change_index) / 32.0); // 2

  for (int ii = 0; ii < target_size; ++ii) {
    arr[ii] = ~0;
  }

  int target_index = change_index % 32;
  arr[target_size] = ~((~0) << target_index);
}

// returns 1 if entire bitmap is empty and 0 if there exists a 1 somewhere within the 
// array of integers
long
is_empty_bitmap(bitmap bb) {
  int size = (int) ceil(((double) bb.num_bits) / 32.0);
  for (int ii = 0; ii < size; ++ii) {
    if (bb.arr[ii] != 0) {
      return 0;
    }
  }
  return 1;
}

// int
// main(int _argc, char* _argv[]) {
//   // START
//   bitmap* bb = malloc(sizeof(bitmap));

//   bb->num_bits = 8 * 32;
//   int size = (int) ceil(((double) bb->num_bits) / 32.0);
//   bb->arr = malloc(sizeof(uint32_t) * size);

//   int32_t* arr = bb->arr;
//   set_val(bb, 31);

//   long index = find_free_bit(*bb);
//   printf("first free bit is %ld\n", index);
//   print_val(arr, 31);

  // uint32_t val2 = 2147483647;
  // uint32_t res2 = ~val2 & (val2 + 1);
  // printf("oper: %u\n", res2);

  // long res2l = (long) res2;
  // long op2 = log((double)res2l)/log(2.0);
  // printf("oper: %ld\n", op2);

  // uint8_t val3 = 127;
  // uint8_t res3 = ~val3 & (val3 + 1); // expecting 128
  // printf("oper: %u\n", res3);


  // long res3l = (long) res3;
  // long op3 = log((double)res3l)/log(2.0);
  // printf("oper: %ld\n", op3);
// }

/// [0 0 1 0 0] => short short
//  [1 1 0 1 0] => MASK [0 0 0 1 0]
//  [1 1 0 0 0]
//gcc -g -o bitmap bitmap.c