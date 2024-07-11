#include <stdio.h>
#include <stdlib.h>
#include "os.h"

void clear_screen(void) {
  const char* CMD =
#if defined WINDOWS
  "CLS";
#elif defined LINUX
  "clear";
#else
  NULL;
#endif

  if (CMD == NULL || system(CMD) != 0) {
    // fake a new screen
    puts("\n----------------------------------------------------------------\n");
  }
}

#ifdef WINDOWS

#pragma warning (disable: 5105)

#include <limits.h>
#include <Windows.h>

// https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps

void start_timer(TIMER* t) {
  LARGE_INTEGER i;
  QueryPerformanceFrequency(&i);
  t->freq = i.QuadPart;
  QueryPerformanceCounter(&i);
  t->start = i.QuadPart;
}

void stop_timer(TIMER* t) {
  LARGE_INTEGER i;
  QueryPerformanceCounter(&i);
  t->stop = i.QuadPart;
}

long long elapsed_usec(const TIMER* t) {
  const long long MILLION = 1000000;
  long long ticks = t->stop - t->start;
  if (ticks < 0 || ticks > LLONG_MAX / MILLION)
    return 0;
  ticks *= MILLION;
  return ticks / t->freq;
}
#endif // WINDOWS
