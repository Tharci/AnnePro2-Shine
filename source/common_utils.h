#pragma once

#include "ch.h"


#define LEN(a) (sizeof(a)/sizeof(*a))

systime_t sysTimeMs(void);
systime_t sysTimeS(void);


unsigned long randInt(void);


