// Minimal test macros. Unlike assert(), they stay active under NDEBUG and exit
// with status 1 and a message instead of opening a Debug-CRT dialog.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

#define CHECK(condition)                                                          \
  do                                                                              \
  {                                                                               \
    if (!(condition))                                                             \
    {                                                                             \
      std::fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #condition, __FILE__, __LINE__); \
      std::exit(1);                                                               \
    }                                                                             \
  } while (false)

// Statement must throw std::exception whose message contains `fragment`.
#define CHECK_THROWS(statement, fragment)                                         \
  do                                                                              \
  {                                                                               \
    bool thrown_ = false;                                                         \
    try { statement; }                                                            \
    catch (const std::exception& e_)                                              \
    {                                                                             \
      thrown_ = true;                                                             \
      if (std::string(e_.what()).find(fragment) == std::string::npos)             \
      {                                                                           \
        std::fprintf(stderr, "wrong error for `%s` (%s:%d): %s\n", #statement, __FILE__, __LINE__, e_.what()); \
        std::exit(1);                                                             \
      }                                                                           \
    }                                                                             \
    if (!thrown_)                                                                 \
    {                                                                             \
      std::fprintf(stderr, "no exception from `%s` (%s:%d)\n", #statement, __FILE__, __LINE__); \
      std::exit(1);                                                               \
    }                                                                             \
  } while (false)
