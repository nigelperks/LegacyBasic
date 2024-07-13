// Legacy BASIC
// Copyright (c) 2022-24 Nigel Perks

#include <stdbool.h>
#include <signal.h>
#include "interrupt.h"

bool interrupted;

static void interrupt(int sig) {
  interrupted = true;
}

void trap_interrupt(void) {
  interrupted = false;
  signal(SIGINT, &interrupt);
}

void untrap_interrupt(void) {
  signal(SIGINT, SIG_DFL);
}
