// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks

#pragma once

#include "source.h"
#include "symbol.h"
#include "bcode.h"

BCODE* parse_source(const SOURCE*, SYMTAB*, bool recognise_keyword_prefixes);

bool name_is_print_builtin(const char* name);
