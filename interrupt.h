#pragma once

extern bool interrupted;

void trap_interrupt(void);
void untrap_interrupt(void);
