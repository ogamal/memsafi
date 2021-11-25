/**
 * @file library.h
 * @author Osama Attia (osama.gma@gmail.com)
 * @brief 
 * @version 0.1
 * @date Thu Sep 16 23:23:02 PDT 2021
 * 
 * @copyright Copyright (c) 2021
 */

#pragma once

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <dlfcn.h>
#include <ctime>
#include <locale>
#include <vector>


////////////////////////////////////////////////////////////////////////////////
// Local Includes
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Function Prototypes
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Type definitions
////////////////////////////////////////////////////////////////////////////////
using MallocFnType = void* (*)(size_t size);
using CallocFnType = void* (*)(size_t num, size_t size);
using ReallocFnType = void* (*)(void* ptr, size_t size);
using FreeFnType = void (*)(void*);
using MainFnTpe = int (*)(int, char **, char **);


////////////////////////////////////////////////////////////////////////////////
// Pre-processor constants
////////////////////////////////////////////////////////////////////////////////
#define TIME_STR_BUFFER_SIZE 80


////////////////////////////////////////////////////////////////////////////////
// Global Variables
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Classes
////////////////////////////////////////////////////////////////////////////////


/**
 * @brief Struct that gather all params/controls for MemSafi in single place
 * 
 */
struct SafiControl
{
 public:
  bool debug = false;
  bool pending_init = false;

  MallocFnType orig_malloc = nullptr;
  CallocFnType orig_calloc = nullptr;
  ReallocFnType orig_realloc = nullptr;
  FreeFnType orig_free = nullptr;
  MainFnTpe orig_main = nullptr;

  /**
   * @brief Capture the pointers to the original pointers using dlsym
   */
  void init() {
    pending_init = true;
    orig_malloc = (MallocFnType) dlsym(RTLD_NEXT, "malloc");
    orig_calloc = (CallocFnType) dlsym(RTLD_NEXT, "calloc");
    orig_realloc = (ReallocFnType) dlsym(RTLD_NEXT, "realloc");
    orig_free = (FreeFnType) dlsym(RTLD_NEXT, "free");

    if (orig_malloc == nullptr || orig_calloc == nullptr || orig_realloc == nullptr || orig_free == nullptr) {
      fprintf(stderr, "[ERROR] Failed to hook calls: %s\n", dlerror());
      exit(1);
    }
    pending_init = false;
  }
};


/**
 * @brief Struct that acts as data store for MemSafi statistics 
 */
struct SafiStats
{
 public:
  // Thread-safe
  void log_malloc(const size_t size)
  {
    log_alloc_helper(size);
    ++m_num_mallocs;
  }

  // Thread-safe
  void log_calloc(const size_t size)
  {
    log_alloc_helper(size);
    ++m_num_callocs;
  }

  // Thread-safe
  void log_realloc(const size_t size)
  {
    log_alloc_helper(size);
    ++m_num_reallocs;
  }

  // Thread-safe
  void log_free(const size_t size)
  {
    m_reserved -= size;
    m_freed += size;
    ++m_num_frees;
  }


  void print(FILE* stream=stderr) const
  {
    char time_buffer[TIME_STR_BUFFER_SIZE];
    std::time_t current_time = std::time(nullptr);
    std::strftime(time_buffer, TIME_STR_BUFFER_SIZE, "%c %Z", std::localtime(&current_time));

    auto print_size = [stream] (const char* prefix, int64_t size) {
      const char* units[] = {"B", "kB", "MB", "GB", "TB"};
      int length = sizeof(units) / sizeof(units[0]);

      int i = 0;
      for (i = 0; (size / 1024) > 0 && i < length - 1; i++) {
          size /= 1024;
      }
      fprintf(stream, "%s %ld %s\n", prefix, size, units[i]);
    };

    fprintf(stream, "\n\n>>>>>>>>>>>>> %s <<<<<<<<<<<\n", time_buffer);
    fprintf(stream, "Overall stats (with alignement):\n");

    print_size("Currently reserved:", m_reserved.load());
    fprintf(stream, "\n");

    print_size("Peak memory:", m_real_peak.load());
    print_size("Total reserved:", m_total_reserved.load());
    print_size("Total freed:", m_freed.load());
    fprintf(stream, "\n");

    fprintf(stream, "Number of mallocs: %ld\n", m_num_mallocs.load());
    fprintf(stream, "Number of callocs: %ld\n", m_num_callocs.load());
    fprintf(stream, "Number of reallocs: %ld\n", m_num_reallocs.load());
    fprintf(stream, "Number of frees: %ld\n", m_num_frees.load());

    fprintf(stream, "\n");
  }
  
  void disablePrint() { m_disable_print = true; }
  bool isPrintEnabled() const { return m_disable_print; }

 private:
  std::atomic<int64_t> m_reserved {0}; // Bytes
  std::atomic<int64_t> m_total_reserved {0}; // Bytes
  std::atomic<int64_t> m_real_peak {0}; // Bytes
  std::atomic<int64_t> m_freed {0}; // Bytes

  std::atomic<int64_t> m_num_mallocs {0};
  std::atomic<int64_t> m_num_callocs {0};
  std::atomic<int64_t> m_num_reallocs {0};
  std::atomic<int64_t> m_num_frees {0};

  bool m_enable_trace {false};
  bool m_disable_print {false};

  // Thread-safe except peak calculations!
  void log_alloc_helper(const size_t size)
  {
    m_reserved += size;
    m_total_reserved += size;

    // TODO Probaly need a mutex for this instead of std::atomic to be truly thread-safe!
    m_real_peak = std::max(m_real_peak.load(), m_reserved.load());
  }
};
