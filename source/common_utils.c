#include "common_utils.h"


systime_t sysTimeMs() {
  return chVTGetSystemTime() / 10 + 1000000;
}

systime_t sysTimeS() {
    return chVTGetSystemTime() / 10000 + 1000;
}


static unsigned long rand_x=123456789, rand_y=362436069, rand_z=521288629;

unsigned long randInt() {
    unsigned long t;
    rand_x ^= rand_x << 16;
    rand_x ^= rand_x >> 5;
    rand_x ^= rand_x << 1;

   t = rand_x;
   rand_x = rand_y;
   rand_y = rand_z;
   rand_z = t ^ rand_x ^ rand_y;

  return rand_z;
}
