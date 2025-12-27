#include "InterpDelay.hpp"

float DSY_SDRAM_BSS sdramData[50][144000];
unsigned int count = 0;
bool triggerClear;
double clearPopCancelValue = 1.;
double hold = 0.;