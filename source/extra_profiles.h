#ifndef EXTRA_PROFILES_H
#define EXTRA_PROFILES_H

#include "light_utils.h"

void anim_rain(led_t* keyColors);
void rain_init(void);
void anim_thunder(led_t* keyColors);
void anim_breathing(led_t* keyColors);
void breathing_init(void);
void pressed_breathing(uint8_t x, uint8_t y, led_t* keyColors);
void anim_snowing(led_t* keyColors);
void snowing_init(void);
void anim_locked(led_t* ledColors);
void locked_init(void);
void anim_stars(led_t* ledColors);


#endif // EXTRA_PROFILES_H
