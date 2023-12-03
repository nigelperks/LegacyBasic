#include <stdlib.h>
#include "os.h"

void clear_screen(void) {
#if defined WINDOWS
  system("CLS");
#elif defined LINUX
  system("clear");
#endif
}
