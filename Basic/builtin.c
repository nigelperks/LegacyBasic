// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks

#include <string.h>
#include "builtin.h"
#include "bcode.h"
#include "utils.h"
#include "os.h"

// used in test_insert_builtins() in paren.c
// parameter types: n=number, s=string, d=dummy
BUILTIN builtins[] = {
  { "ABS",    TYPE_NUM, "n",   B_ABS },
  { "ASC",    TYPE_NUM, "s",   B_ASC },
  { "ATN",    TYPE_NUM, "n",   B_ATN },
  { "CHR$",   TYPE_STR, "n",   B_CHR },
  { "COS",    TYPE_NUM, "n",   B_COS },
  { "EXP",    TYPE_NUM, "n",   B_EXP },
  { "FIX",    TYPE_ERR, NULL,  B_NOP },
  { "GET$",   TYPE_ERR, NULL,  B_NOP },
  { "HEX$",   TYPE_ERR, NULL,  B_NOP },
#if HAS_KBHIT && HAS_GETCH
  { "INKEY$", TYPE_STR, "d",   B_INKEY },
#else
  { "INKEY$", TYPE_ERR, NULL,  B_NOP },
#endif
  { "INT",    TYPE_NUM, "n",   B_INT },
  { "LEFT$",  TYPE_STR, "sn",  B_LEFT },
  { "LEN",    TYPE_NUM, "s",   B_LEN },
  { "LOG",    TYPE_NUM, "n",   B_LOG },
  { "MID$",   TYPE_STR, "snn", B_MID3 },
  { "RIGHT$", TYPE_STR, "sn",  B_RIGHT },
  { "RND",    TYPE_NUM, "d",   B_RND },
  { "SGN",    TYPE_NUM, "n",   B_SGN },
  { "SIN",    TYPE_NUM, "n",   B_SIN },
  { "SPACE$", TYPE_ERR, NULL,  B_NOP },
  { "SQR",    TYPE_NUM, "n",   B_SQR },
  { "STR$",   TYPE_STR, "n",   B_STR },
  { "TAN",    TYPE_NUM, "n",   B_TAN },
  { "TIME$",  TYPE_STR, "d",   B_TIME_STR },
  { "USR",    TYPE_ERR, NULL,  B_NOP },
  { "VAL",    TYPE_NUM, "s",   B_VAL },
};

const BUILTIN* builtin(const char* s) {
  for (int i = 0; i < sizeof builtins / sizeof builtins[0]; i++) {
    if (STRICMP(builtins[i].name, s) == 0)
      return &builtins[i];
  }

  return NULL;
}

const BUILTIN* builtin_number(unsigned i) {
  if (i < sizeof builtins / sizeof builtins[0])
    return &builtins[i];

  return NULL;
}
