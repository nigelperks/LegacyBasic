// Legacy BASIC
// Copyright (c) 2022 Nigel Perks

#pragma once

#include "source.h"
#include "bcode.h"

BCODE* parse(const SOURCE*, bool recognise_keyword_prefixes);