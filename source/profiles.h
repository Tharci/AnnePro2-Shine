#pragma once

#include "light_utils.h"


#define REACTIVE_FPS 0


typedef void (*anim_tick)( led_t* );
typedef void (*anim_keypress)( uint8_t col, uint8_t row, led_t* keyColors );
typedef void (*anim_init)( led_t* );

typedef struct {
  uint8_t fps;
  anim_tick tick;
  anim_init init;
  anim_keypress keypress;
} Profile;


// tick function for one-shot effects
// returns false if effect is done
typedef bool (*oneShot_tick)( led_t* );

typedef struct {
    anim_init init; // == 0 when disabled
    oneShot_tick tick; 
    uint8_t fps;
} Effect;


//////

void blendColors(led_t* color1, led_t* color2, led_t* dest);

//////


uint8_t getReactiveFps(void);

void animatedRainbowFlow(led_t* ledColors);

void prof_rain_init(led_t* ledColors);
void prof_rain_tick(led_t* keyColors);

void prof_storm_init(led_t* ledColors);
void prof_storm_tick(led_t* keyColors);

void prof_breathing_init(led_t* ledColors);
void prof_breathing_tick(led_t* keyColors);
void prof_breathing_pressed(uint8_t x, uint8_t y, led_t* keyColors);

void prof_snowing_init(led_t* ledColors);
void prof_snowing_tick(led_t* keyColors);

void prof_locked_init(led_t* ledColors);
void prof_locked_tick(led_t* ledColors);

void prof_stars_tick(led_t* ledColors);

void prof_sunny_init(led_t* ledColors);
void prof_sunny_tick(led_t* ledColors);

void prof_cloudy_init(led_t* ledColors);
void prof_cloudy_tick(led_t* ledColors);


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

void prof_liveWeather_init(led_t* ledColors);
void prof_liveWeather_tick(led_t* ledColors);
void setWeatherData(WeatherData* data);
WeatherData* getWeatherData(void);
bool weatherIsUpToDate(void);
Time getCurrentTime(void);

void prof_blink_init(led_t* ledColors);
void prof_blink_tick(led_t* ledColors);

void prof_weatherShowoff_init(led_t* ledColors);
void prof_weatherShowoff_tick(led_t* ledColors);



void effect_weave_green_init(led_t* ledColors);
void effect_weave_yellow_init(led_t* ledColors);
void effect_weave_red_init(led_t* ledColors);
bool effect_weave_tick(led_t* ledColors);

void effect_blt_init(uint8_t bltState);
void effect_blinking_disable(void);
void effect_blinking_set_speed(systime_t speed);
void effect_blinking_init(led_t* ledColors);
bool effect_blinking_tick(led_t* ledColors);




