// Legacy BASIC
// Utility types and functions.
// Copyright (c) 2022-3 Nigel Perks

#pragma once

#if defined LINUX || defined linux
#define STRICMP strcasecmp
#define STRNICMP strncasecmp
#elif defined _WINDOWS || defined _WIN32 || defined _WIN64
#define STRICMP _stricmp
#define STRNICMP _strnicmp
#else
#error unknown operating system
#endif
