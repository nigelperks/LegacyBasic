// Legacy BASIC
// Utility types and functions.
// Copyright (c) 2021-2 Nigel Perks

#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdbool.h>

extern const char* progname;

typedef unsigned short U16;

void fatal(const char* fmt, ...);

void* emalloc(size_t);
void* erealloc(void*, size_t);
void* ecalloc(size_t count, size_t size);
char* estrdup(const char*);

void efree(void*);

extern unsigned long malloc_count, free_count;

enum { TYPE_ERR, TYPE_NUM, TYPE_STR };

bool string_name(const char*);

void space(unsigned count, FILE*);

#endif // UTILS_H
