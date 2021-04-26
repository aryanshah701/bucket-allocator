#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <math.h>

#include "hmalloc.h"
#include "bitmap.h"

// Bucket Allocator

typedef struct free_block {
    size_t bucket_size;
    struct free_block* next;
    struct free_block* prev;
    bitmap* bb;
    int arena_idx;
    int used;
} free_block;

typedef struct arena {
  pthread_t thread_id;
  free_block* buckets[14];
} arena;

typedef struct header {
    size_t size;
} header;

const size_t PAGE_SIZE = 4096;

const size_t MAX_SIZE = 1056;

const int NUM_BUCKETS = 14;

const int NUM_ARENAS = 100;

// The arenas
arena arenas[100];

// Lock
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void 
check_rv(int rv, char* msg) {
    if (rv == -1) {
        perror("oops");
        puts(msg);
        fflush(stdout);
        fflush(stderr);
        abort();
    }
}

static
size_t
div_up(size_t xx, size_t yy)
{
    // This is useful to calculate # of pages
    // for large allocations.
    size_t zz = xx / yy;

    if (zz * yy == xx) {
        return zz;
    }
    else {
        return zz + 1;
    }
}

// Function to compute the bucket index given the number of bytes to allocate
int
get_bucket_num(size_t bytes) {
  if (bytes <= 16) {
      return 0;
  }

  double ll = log(bytes) / log(2);
  double lower_bound = floor(ll);
  double upper_bound = ceil(ll);

  int rr = (int)(2 * (upper_bound - 4));

  if ((int)lower_bound == (int)upper_bound) {
      return rr;
  }

  int average = (int)((pow(2.0, lower_bound) + pow(2.0, upper_bound))/2.0);
  if (bytes <= average) {
      int bucket_num = rr - 1;
      return bucket_num; 
  } else {
      return rr;
  }
}

// Function to compute the size of a bucket given an even bucket idx
int 
get_bucket_size_even(int bucket_num) {
  double nn = ((double)bucket_num / 2.0) + 4;
  int rr = (int)pow(2.0, (double)nn);
  return rr;
}

// Function to compute the size of a bucket given the bucket idx
int
get_bucket_size(int bucket_num) {
  if (bucket_num % 2 == 0) {
    return get_bucket_size_even(bucket_num);
  } else {
    int v1 = get_bucket_size_even(bucket_num - 1);
    int v2 = get_bucket_size_even(bucket_num + 1);
    int avg = (v1 + v2) / 2;
    return avg;
  }
}

// Compute the size of a single block given the size of each
// allocatable free region of memory for the bucket
int
compute_block_size(int bucket_size) {
  return PAGE_SIZE;
}

size_t
compute_bitmap_array_size(int num_bits) {
  double int32_in_bits = sizeof(int32_t) * 8;
  double num_ints = ceil((double)num_bits / int32_in_bits);
  size_t arr_size = sizeof(int32_t) * num_ints;
  return arr_size;
}

// Compute the number of bits needed to represent all the
// allocatable free regions of memory for the given bucket
int
compute_num_bits(int bucket_size, int block_size) {
   int max_num_bits = block_size / bucket_size;

   // Assuming we are using the max number of bits
   size_t bitmap_arr_size = compute_bitmap_array_size(max_num_bits);
   int available_space = block_size - (sizeof(free_block) + bitmap_arr_size + sizeof(bitmap));
   int num_bits = available_space / bucket_size;

   return num_bits;
}

// Initializes the free_block with the starting bytes being
// used for the bitmap array.
free_block*
init_new_block(int bucket_size, int block_size, int arena_idx) {
  // MMAP a new block
  bitmap* bb = mmap(0, block_size, 
                PROT_READ|PROT_WRITE, 
                MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  check_rv((long)bb, "mmap (init_new_block)");

  // Initialize the bitmap
  bb->num_bits = compute_num_bits(bucket_size, block_size);
  bb->arr_size = compute_bitmap_array_size(bb->num_bits);
  bb->arr = (int32_t*)((uintptr_t)bb + sizeof(bitmap));

  // The new block
  free_block* new_block = (free_block*)((uintptr_t)bb + sizeof(bitmap) + bb->arr_size);
  new_block->bucket_size = bucket_size;
  new_block->bb = bb;
  new_block->arena_idx = arena_idx;
  new_block->used = 0;

  return new_block;
}

int
get_empty_arena() {
  for (int ii = 0; ii < NUM_ARENAS; ++ii) {
    if (arenas[ii].thread_id == 0) {
      return ii;
    }
  }

  return -1;
}

int
get_arena_idx(pthread_t thread_id) {
  for (int ii = 0; ii < NUM_ARENAS; ++ii) {
    if (arenas[ii].thread_id != 0 && pthread_equal(arenas[ii].thread_id, thread_id)) {
      return ii;
    }
  }

  return -1;
}

void*
xmalloc(size_t bytes)
{
    // After max size, simply MMAP
    if (bytes > MAX_SIZE) {
      bytes += sizeof(header);
      // MMAP and get user_memory
      size_t num_pages = div_up(bytes, PAGE_SIZE);
      size_t allocated_size = num_pages * PAGE_SIZE;
      header* user_memory = mmap(0, allocated_size, 
              PROT_READ|PROT_WRITE, 
              MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
      check_rv((long)user_memory, "mmap (xmalloc, > 4096)");
      
      // Add amount of memory mapped and return
      user_memory->size = (num_pages * PAGE_SIZE) - sizeof(header); 
      user_memory = (void*)((uintptr_t)user_memory + sizeof(header));
      return user_memory;
    }

    // Minimum allocation size if 16 bytes
    bytes = bytes < 16 ? 16 : bytes;

    // Figure out which bucket this goes in
    int bucket_num = get_bucket_num(bytes);

    // Lock
    pthread_mutex_lock(&lock);

    // Figure out which arena this goes in
    pthread_t thread_id = pthread_self();
    int arena_idx = get_arena_idx(thread_id);

    // If no arena has been assigned yet, then assign one
    if (arena_idx == -1) {
      arena_idx = get_empty_arena();

      arenas[arena_idx].thread_id = thread_id;
    }

    free_block** buckets = arenas[arena_idx].buckets;

    
    // Look for an available free block 
    free_block* curr = buckets[bucket_num];
    free_block* prev = 0;
    int bit_idx = -1;

    while (curr) {
      // If PAGE isn't full, look for nearest available bit
      if (curr->used < curr->bb->num_bits) {
        // Look for available space in this block
        bit_idx = find_free_bit(*curr->bb);

        // If a free spot if available
        if (bit_idx != -1 && bit_idx < curr->bb->num_bits) {
          break;
        } 
      }

      prev = curr;
      curr = curr->next;
    }

    // If there isn't any available free blocks
    if (!curr) {
      // Create a new free block and push it into the bucket

      // Compute the size of each free_list block in the bucket
      int bucket_size;
      if (prev) {
        bucket_size = prev->bucket_size;
      } else {
        bucket_size = get_bucket_size(bucket_num);
      }

      // Compute the size of each block in the bucket
      int block_size = compute_block_size(bucket_size);

      // Intialize the new free block
      curr = init_new_block(bucket_size, block_size, arena_idx);

      // Push it onto the end of the bucket
      if (prev) {
        prev->next = curr;
        curr->prev = prev;
      } else {
        curr->prev = 0;
        buckets[bucket_num] = curr;
      }

      curr->next = 0;
    }

    // Compute the pointer to the available user memory
    bit_idx = bit_idx == -1 ? 0 : bit_idx;
    size_t bucket_size = curr->bucket_size;
    void* user_memory = (void*)((uintptr_t)curr + 
                                sizeof(free_block) + 
                                (bit_idx * bucket_size)); 

    // Update the bitmap bit to mark it as occupied
    change_bit(*curr->bb, bit_idx, 1);
    curr->used += 1;

    // UNLOCK 
    pthread_mutex_unlock(&lock);

    return user_memory;
}

free_block*
get_header(void* ptr) {
  // Pointer arithmetic to figure out the size
  long num_pages = (uintptr_t)ptr / PAGE_SIZE;
  bitmap* bb = (void*)(num_pages * PAGE_SIZE);
  free_block* header = (free_block*)((uintptr_t)bb + sizeof(bitmap) + bb->arr_size);
  return header;
}

// Coalesces and unmaps a free block
void
remove_free_block(free_block* hh, int arena_idx) {
  free_block** buckets = arenas[arena_idx].buckets;

  // Coalesce
  free_block* prev = hh->prev;
  free_block* next = hh->next;
  
  if (prev) {
    prev->next = next;
  } else {
    int bucket_num = get_bucket_num(hh->bucket_size);
    buckets[bucket_num] = next;
  }

  if (next) {
    next->prev = prev;
  }

  // Unmap
  int rv = munmap(hh->bb, PAGE_SIZE);
  check_rv(rv, "munmap (remove free block)");
}

// Checks whether the given pointer falls within the range
// of any bucket allocated memory
int
is_bucket_allocated(void* ptr) {
  for (int xx = 0; xx < NUM_ARENAS; ++xx) {
    free_block** buckets = arenas[xx].buckets;
    for (int ii = 0; ii < NUM_BUCKETS; ++ii) {
      // Is the pointer in this bucket
      free_block* curr = buckets[ii];
      while (curr) {
        // Is the pointer in this block
        uintptr_t min = (uintptr_t)curr + sizeof(free_block);
        uintptr_t max = (uintptr_t)curr->bb + PAGE_SIZE;
        uintptr_t int_ptr = (uintptr_t)ptr;

        if (int_ptr >= min && int_ptr < max) {
          return 1;
        }

        // Move to the next block
        curr = curr->next;
      }
    }
  }

  // Pointer not found to be in a valid bucket range
  return 0;
}

void
xfree(void* ptr)
{
    // Lock
    pthread_mutex_lock(&lock);

    // Check if this isn't a bucket allocated block of memory
    if (!is_bucket_allocated(ptr)) {
      // Unlock 
      pthread_mutex_unlock(&lock);

      // Simply unmap the ptr
      header* hh = (header*)((uintptr_t)ptr - sizeof(header));
      size_t size = hh->size;
      
      int rv = munmap(hh, size);
      check_rv(rv, "munmap (free)");

      return;
    }

    // Get the header of the page
    free_block* hh = get_header(ptr);

    // Get the idx of the arena this block is being freed from
    int arena_idx = hh->arena_idx;

    // Set that bit to 0 so that it is available for use again, decrement used count
    size_t bucket_size = hh->bucket_size;
    int bit_idx = ((uintptr_t)ptr - (uintptr_t)hh - sizeof(free_block)) / bucket_size;
    change_bit(*hh->bb, bit_idx, 0);
    hh->used -= 1;

    // If the bitmap is now completely unused, remove it
    if(is_empty_bitmap(*hh->bb)) {
      remove_free_block(hh, arena_idx);
    }

    // Unlock 
    pthread_mutex_unlock(&lock);
}

void*
xrealloc(void* prev, size_t bytes)
{
    // Lock
    pthread_mutex_lock(&lock);

    // Check if this isn't a bucket allocated block of memory
    if (!is_bucket_allocated(prev)) {
      // Unlock
      pthread_mutex_unlock(&lock);

      // Simply unmap the ptr
      header* hh = (header*)((uintptr_t)prev - sizeof(header));
      size_t previous_size = hh->size;

      // malloc the new memory block
      void* new_memory = xmalloc(bytes);
      
      // Copy over the data
      memcpy(new_memory, prev, previous_size);
      
      // Unmap the old memory
      int rv = munmap(hh, previous_size);
      check_rv(rv, "munmap (free)");

      return new_memory;
    }

    // Get the previous size of memory from the header
    free_block* hh = get_header(prev);
    size_t previous_size = hh->bucket_size;

    // Unlock
    pthread_mutex_unlock(&lock);

    // malloc the new memory block
    void* new_memory = xmalloc(bytes);
    
    // Copy over the data
    memcpy(new_memory, prev, previous_size);

    // free the old memory block
    xfree(prev);

    return new_memory;
}


