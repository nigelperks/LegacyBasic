// Legacy BASIC
// Copyright (c) 2022 Nigel Perks

#pragma once

#include "source.h"
#include "bcode.h"

BCODE* parse_source(const SOURCE*, STRINGLIST* names, bool recognise_keyword_prefixes);
BCODE* parse_text(const char* text, STRINGLIST* names, const char* name, bool recognise_keyword_prefixes);
