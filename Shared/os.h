// Legacy BASIC
// Platform-dependent types and functions.
// Copyright (c) 2022-24 Nigel Perks

#pragma once

#include <string.h>

#ifndef LINUX
#if defined linux
#define LINUX
#endif
#endif

#ifndef WINDOWS
#if defined _WINDOWS || defined _WIN32 || defined _WIN64
#define WINDOWS
#endif
#endif

#if defined LINUX
#define STRICMP strcasecmp
#define STRNICMP strncasecmp
#define HAS_KBHIT 0
#define HAS_GETCH 0
#define HAS_TIMER 0
#elif defined WINDOWS
#include <conio.h>
#define HAS_KBHIT 1
#define HAS_GETCH 1
#define HAS_TIMER 1
#define STRICMP _stricmp
#define STRNICMP _strnicmp
#else
#error unknown operating system
#endif

void clear_screen(void);

#if HAS_TIMER
typedef struct {
  long long freq;
  long long start;
  long long stop;
} TIMER;

void start_timer(TIMER*);
void stop_timer(TIMER*);
long long elapsed_usec(const TIMER*);
#endif // HAS_TIMER
