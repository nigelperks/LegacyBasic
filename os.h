// Legacy BASIC
// Utility types and functions.
// Copyright (c) 2021-2 Nigel Perks

#ifndef OS_H
#define OS_H

#ifdef LINUX
#define STRICMP strcasecmp
#define STRNICMP strncasecmp
#else
#define STRICMP _stricmp
#define STRNICMP _strnicmp
#endif

#endif // OS_H
