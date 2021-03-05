#ifndef EXTRA_PROFILES_H
#define EXTRA_PROFILES_H

#include "light_utils.h"


uint8_t getReactiveFps(void);

void rain_init(led_t* ledColors);
void anim_rain(led_t* keyColors);

void storm_init(led_t* ledColors);
void anim_storm(led_t* keyColors);

void breathing_init(led_t* ledColors);
void anim_breathing(led_t* keyColors);
void pressed_breathing(uint8_t x, uint8_t y, led_t* keyColors);

void snowing_init(led_t* ledColors);
void anim_snowing(led_t* keyColors);

void locked_init(led_t* ledColors);
void anim_locked(led_t* ledColors);

void anim_stars(led_t* ledColors);

void sunny_init(led_t* ledColors);
void anim_sunny(led_t* ledColors);

void cloudy_init(led_t* ledColors);
void anim_cloudy(led_t* ledColors);

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} Time;

typedef struct {
    Time time;
    Time sunriseTime;
    Time sunsetTime;
    int8_t temp;
    int8_t tempMin;
    int8_t tempMax;
    uint8_t sunIntensity;
    uint8_t cloudDensity;
    uint8_t windIntensity;
    uint8_t rainIntensity;
    uint8_t stormIntensity;
    uint8_t snowIntensity;
    bool mist;
} WeatherData;

void liveWeather_init(led_t* ledColors);
void anim_liveWeather(led_t* ledColors);
void setWeatherData(WeatherData* data);


#endif // EXTRA_PROFILES_H
