/**
 * @file library.cpp
 * @author Osama Attia (osama.gma@gmail.com)
 * @brief The implementation of a shared library that uses LD_PRELOAD to trace
 *        the malloc, calloc, realloc and free calls. Then, print stats every 
 *        few seconds to stderr.
 * @version 0.1
 * @date Thu Sep 16 23:23:02 PDT 2021
 * 
 * @copyright Copyright (c) 2021
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>


////////////////////////////////////////////////////////////////////////////////
// Local Includes
////////////////////////////////////////////////////////////////////////////////
#include "library.h"


////////////////////////////////////////////////////////////////////////////////
// Function Prototypes
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Pre-processor constants
////////////////////////////////////////////////////////////////////////////////
#define TEMP_BUFFER_SIZE 80000
#define PRINT_FREQ_IN_SEC 5

////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Global Variables
////////////////////////////////////////////////////////////////////////////////
SafiStats safiStats;
SafiControl safiControl;
std::thread* print_thread;

// Temp space that is used to allocate memory at initiation while dlsym is pending
char temp_buffer[TEMP_BUFFER_SIZE];
size_t used_buffer_size = 0;


////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Function that is designed to run in a thread to print memory
 * statistics periodically (every 5 seconds)
 */
static void __print_safi_stats()
{
  while (!safiStats.isPrintEnabled()) {
    std::this_thread::sleep_for(std::chrono::seconds(PRINT_FREQ_IN_SEC));
    safiStats.print();
  }
}


/**
 * @brief Print debug message if debug flag is set
 * 
 * @param format Message formatted in printf style format
 * @param ... veriadic arguments
 */
void __debug_msg(const char* format, ...)
{
  if (!safiControl.debug) {
    return;
  }

  va_list argptr;
  va_start(argptr, format);
  vfprintf(stderr, format, argptr);
  va_end(argptr);
}


/**
 * @brief A thread-safe temporary malloc function that could be used by dlsym for bootstraping
 */
static void* __temp_malloc(size_t size)
{
  static std::mutex buffer_mutex;
  std::lock_guard<std::mutex> guard(buffer_mutex);

  __debug_msg("[INFO] Temp Malloc (size: %lu)\n", size);
  if (used_buffer_size + size > TEMP_BUFFER_SIZE) {
    __debug_msg("[ERROR] Temp Malloc calls required more than the set max!\n");
    exit(1);
  }

  void* p = (void*)(temp_buffer + used_buffer_size);
  used_buffer_size += size;
  return p;
}


/**
 * @brief Capture the original function pointers on first use
 */
static void __init_safi()
{
  char* safi_debug_str = getenv("MEM_SAFI_DEBUG");
  if (safi_debug_str != nullptr && strcmp(safi_debug_str, "1") == 0) {
    safiControl.debug = true;
  }

  __debug_msg("[INFO] Start Init!\n");
  safiControl.init();

  // Spwan a thread to print stats
  print_thread = new std::thread(__print_safi_stats);

  __debug_msg("[INFO] End Init!\n");
}

extern "C" {
/**
 * @brief Wrapper function for the original GLIBC 'malloc' function
 * 
 * @param size 
 * @return void* 
 */
void* malloc(size_t size)
{
  __debug_msg("[INFO] Malloc call (size: %lu)\n", size);

  // Help dlsym to allocate some memory using a temporary buffer!
  if (safiControl.pending_init) {
    return __temp_malloc(size);
  }

  if (safiControl.orig_malloc == nullptr) {
    __init_safi();
  }

  void* p = safiControl.orig_malloc(size);
  safiStats.log_malloc(malloc_usable_size(p));

  return p;
}


/**
 * @brief Wrapper for the original GLIBC 'calloc' function
 * 
 * @param num 
 * @param size 
 * @return void* 
 */
void* calloc(size_t num, size_t size)
{
  __debug_msg("[INFO] Calloc call (num, %lu, size: %lu)\n", num, size);

  if (safiControl.orig_calloc == nullptr) {
    void* p = malloc(num * size);
    if (p != nullptr) {
      memset(p, 0, num * size);
    }
    return p;
  }

  void* p = safiControl.orig_calloc(num, size);
  safiStats.log_calloc(malloc_usable_size(p));

  return p;
}


/**
 * @brief Wrapper for the original GLIBC 'realloc' function
 * 
 * @param ptr 
 * @param size 
 * @return void* 
 */
void* realloc(void* ptr, size_t size)
{
  __debug_msg("[INFO] Realloc call (ptr, %p, size: %lu)\n", ptr, size);

  if (safiControl.orig_realloc == nullptr) {
    void* new_ptr = malloc(size);
    if (new_ptr != nullptr && ptr != nullptr) {
      memmove(new_ptr, ptr, size);
      free(ptr);
    }
    return new_ptr;
  }

  size_t old_size = malloc_usable_size(ptr);
  void* new_ptr = safiControl.orig_realloc(ptr, size);
  safiStats.log_realloc(malloc_usable_size(new_ptr) - old_size);

  return new_ptr;
}


/**
 * @brief Wrapper function for the original GLIBC 'free' function
 * 
 * @param ptr 
 */
void free(void* ptr)
{
  __debug_msg("[INFO] Free call (ptr: %p)!\n", ptr);
  if (ptr >= (void*)temp_buffer && ptr <= (void*)(temp_buffer + used_buffer_size)) {
    __debug_msg("[INFO] Free pointer allocated by temp Malloc call!\n");
    return;
  }

  if (safiControl.orig_free == nullptr) {
    __init_safi();
  }

  size_t size = malloc_usable_size(ptr);
  safiStats.log_free(size);

  safiControl.orig_free(ptr);
}


/**
 * @brief Wrapper funcatin that replaces the real main
 */
int main_hook(int argc, char** argv, char** envp)
{
  int ret = safiControl.orig_main(argc, argv, envp);

  __debug_msg("[INFO] Actual main function completed (exit code: %d)!\n", ret);

  // Stop reporting statistics
  safiStats.disablePrint();
  if (print_thread != nullptr) {
    print_thread->detach();
    delete print_thread;
  }
  safiStats.print();

  return ret;
}


/**
 * @brief Wrapper for __libc_start_main() that enables wrapping the real main
 */
int __libc_start_main(
    MainFnTpe main,
    int argc,
    char** argv,
    MainFnTpe init,
    void (*fini)(void),
    void (*rtld_fini)(void),
    void* stack_end)
{
  safiControl.orig_main = main;

  // Find the real __libc_start_main()
  using StartMainType = int (*)(
      MainFnTpe,
      int,
      char**,
      MainFnTpe,
      void (*)(void),
      void (*)(void),
      void*);
  StartMainType orig = (StartMainType)dlsym(RTLD_NEXT, "__libc_start_main");

  // Call it with our custom main function
  return orig(main_hook, argc, argv, init, fini, rtld_fini, stack_end);
}

}  // End extern "C"
