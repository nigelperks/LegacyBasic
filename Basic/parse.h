// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks

#pragma once

#include "source.h"
#include "bcode.h"

BCODE* parse_source(const SOURCE*, STRINGLIST* names, bool recognise_keyword_prefixes);

bool name_is_print_builtin(const char* name);
