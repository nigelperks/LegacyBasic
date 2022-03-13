// Legacy BASIC
// Utility types and functions.
// Copyright (c) 2021-2 Nigel Perks

#ifndef OS_H
#define OS_H

#ifdef LINUX
#define __STRICMP strcasecmp
#define __STRNICMP strncasecmp
#else
#define __STRICMP _stricmp
#define __STRNICMP _strnicmp
#endif

#endif // OS_H
