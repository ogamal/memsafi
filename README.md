# Introduction:
**MemSafi** is a poor-man's memory profiler. It uses `LD_PRELOAD` to wrap GLIBC memory calls. The name **MemSafi** is combination of **memory** and the arabic word **Safi** which means **clear**.

The library traces the GLIBC functions below and print statistics about memory usage to `stderr` every 5 seconds:
- `malloc`
- `calloc`
- `realloc`
- `free`

# Build and Usage:
- Build using `make clean && make`
- The shared library will be found in the `build` directory
- You can profile any application using `LD_PRELOAD=build/memsafi.so <app_path> <args>`
- You can also run **MemSafi** library in debug mode using `MEM_SAFI_DEBUG=1 LD_PRELOAD=memsafi.so <app_path> <args>`

## Notes:
- The library uses a local temporary buffer to help DLSYM to allocate memory at initalization.

# To Do Items:
- Add proper testing
  - The library is tested manually against local tests as well as some bash commands like `ls`, `du`, `cat`, ... etc.
- Trace size before alignment
  - This feature requires a hash table implemetation to get freed memory size
  - Alternatively, allocated memory could be increased by 4-8 bytes to store meta-data about size
- Revisit initialization as it might not be thread-safe
- Revisit peak memory calculation as it might not be thread-safe
  - The peak calculation should use `std::atomic` instead of mutex because `std::atomic` doesn't support `max` operation
