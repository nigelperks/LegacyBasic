// Legacy BASIC
// Utility types and functions.
// Copyright (c) 2022-3 Nigel Perks

#ifndef OS_H
#define OS_H

#if defined LINUX || defined linux
#define STRICMP strcasecmp
#define STRNICMP strncasecmp
#elif defined _WINDOWS
#define STRICMP _stricmp
#define STRNICMP _strnicmp
#else
#error unknown operating system
#endif

#endif // OS_H
