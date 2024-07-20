// Legacy BASIC
// Copyright (c) 2024 Nigel Perks
// Symbol hashing.

#include "ctype.h"

// Aho, Sethi, Ullman "Compilers: Principles, Techniques, and Tools" (1986) p. 436

static unsigned hash_in(unsigned h, unsigned c) {
  unsigned g;
  h = (h << 4) + c;
  if (g = h & 0xf0000000) {
    h ^= (g >> 24);
    h ^= g;
  }
  return h;
}

unsigned hashpjw_upper(const char *s) {
  unsigned h = 0;
  for (const char* p = s; *p; p++)
    h = hash_in(h, toupper(*p));
  return h;
}

unsigned hashpjw_upper_len(const char *s, unsigned len) {
  unsigned h = 0;
  for (const char* p = s; len; p++, len--)
    h = hash_in(h, toupper(*p));
  return h;
}
