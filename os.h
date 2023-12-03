// Legacy BASIC
// Utility types and functions.
// Copyright (c) 2022-3 Nigel Perks

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
#error need KBHIT and GETCH for Linux
#define HAS_KBHIT 0
#define HAS_GETCH 0
#elif defined WINDOWS
#include <conio.h>
#define HAS_KBHIT 1
#define HAS_GETCH 1
#define STRICMP _stricmp
#define STRNICMP _strnicmp
#else
#error unknown operating system
#endif

void clear_screen(void);
