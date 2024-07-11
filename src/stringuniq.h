// Maintain a set of unique strings by means of a binary tree.
// Copyright (c) 2023 Nigel Perks

#pragma once

typedef struct unique_strings UNIQUE_STRINGS;

UNIQUE_STRINGS* new_unique_strings(void);
void delete_unique_strings(UNIQUE_STRINGS*);
void insert_unique_string(UNIQUE_STRINGS*, const char*);
void traverse_unique_strings(const UNIQUE_STRINGS*, void (*callback)(const char*));
