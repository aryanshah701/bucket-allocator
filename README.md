# Bucket Memory Allocator

A thread-safe bucket memory allocator for memory management on Linux systems.

Utilizes Arenas for thread safety and a bucket based approach like [Facebook's jemalloc](https://github.com/jemalloc/jemalloc) in order to achieve constant time alllocations and deallocations at the cost of some internal and external fragmentation and large startup allocation.
