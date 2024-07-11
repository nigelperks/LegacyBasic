// Legacy BASIC
// Copyright (c) 2022 Nigel Perks

// String list.
// Copyright (c) 2021 Nigel Perks

#ifndef STRINGLIST_H
#define STRINGLIST_H

typedef struct {
  char* * strings;
  unsigned allocated;
  unsigned used;
} STRINGLIST;

void init_stringlist(STRINGLIST*);
void deinit_stringlist(STRINGLIST*);

STRINGLIST* new_stringlist(void);
void delete_stringlist(STRINGLIST*);

unsigned stringlist_count(const STRINGLIST*);
const char* stringlist_item(const STRINGLIST*, unsigned);
unsigned stringlist_append(STRINGLIST*, const char*);
unsigned name_entry(STRINGLIST*, const char*); // for case-insensitive BASIC

#endif // STRINGLIST_H
