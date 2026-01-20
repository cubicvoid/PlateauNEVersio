#include "InterpDelay.hpp"

float DSY_SDRAM_BSS sdramData[50][144000];
unsigned int _InterpDelayCount = 0;
double clearPopCancelValue = 1.;
double _InterpDelayHold = 0.;