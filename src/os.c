#include <stdio.h>
#include <stdlib.h>
#include "os.h"

void clear_screen(void) {
  const char* CMD =
#if defined WINDOWS
  "CLS";
#elif defined LINUX
  "clear";
#else
  NULL;
#endif

  if (CMD == NULL || system(CMD) != 0) {
    // fake a new screen
    puts("\n----------------------------------------------------------------\n");
  }
}
