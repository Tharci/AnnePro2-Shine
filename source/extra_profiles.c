#include "extra_profiles.h"
#include "miniFastLED.h"
#include "stdlib.h"
#include "ap2_qmk_led.h"



////// LIGHT UTILS //////

static const led_t black   = {0, 0, 0};
static const led_t white   = {200, 255, 255};
static const led_t red     = {255, 25, 0};
static const led_t green   = {0, 255, 30};
static const led_t blue    = {0, 0, 255};
static const led_t pink    = {200, 0, 255};
static const led_t purple  = {50, 0, 255};
static const led_t yellow  = {180, 255, 0};
static const led_t orange  = {255, 140, 0};
static const led_t turkiz  = {0, 255, 255};



static void setAllColors(led_t* ledColors, const led_t* color) {
    for (int i = 0; i < NUM_ROW * NUM_COLUMN; i++) {
        ledColors[i] = *color;
    }
}

static bool legitPosition(const pos_i* pos) {
    return
        pos->x >= 0 && pos->x < NUM_COLUMN &&
        pos->y >= 0 && pos->y < NUM_ROW;
}

static void setColor(led_t* ledColors, const pos_i* pos, const led_t* color) {
    if (legitPosition(pos))
        ledColors[pos->y * NUM_COLUMN + pos->x] = *color;
}


static void multiplyColor(const led_t* color, uint8_t brightness, led_t* color2) {
    color2->red   = (((int)color->red) * brightness / 100);
    color2->green = (((int)color->green) * brightness / 100);
    color2->blue  = (((int)color->blue) * brightness / 100);
}


uint8_t reactiveFps = 30;

uint8_t getReactiveFps() {
    return reactiveFps;
}



////// ANIMATED RAIN //////

const led_t rainColor = {30, 30, 255};

#define raindropsBufferSize 40

static uint8_t raindropCnt = 0;
static pos_i raindrops[raindropsBufferSize];

systime_t nextRainSpawn = 0;
static const uint8_t trailLength = 6;

uint8_t rainIntensity = 50;
const systime_t rainSpeedMs = 55;
systime_t lastMoved = 0;

void rain_init(led_t* ledColors) {
    raindropCnt = 0;
    nextRainSpawn = 0;
    lastMoved = 0;
    rainIntensity = 50;
}

void anim_rain(led_t* ledColors) {
    if (sysTimeMs() - lastMoved > rainSpeedMs) {
        for (size_t i = 0; i < raindropCnt; i++) {
            raindrops[i].y++;
        }
        
        setAllColors(ledColors, &black);
        for (uint8_t i = 0; i < raindropCnt; i++) {
            for (uint8_t y = 0; y < NUM_ROW && y <= raindrops[i].y; y++) {
                if (raindrops[i].y - y < trailLength + 1) {
                    uint8_t brightness = 100 - ((raindrops[i].y - y) * 100 / trailLength);

                    pos_i currPos = { raindrops[i].x, y };
                    led_t multipliedColor;
                    multiplyColor(&rainColor, brightness, &multipliedColor);

                    if (currPos.y >= 0 && currPos.y < NUM_ROW) {
                        ledColors[currPos.y * NUM_COLUMN + currPos.x] = multipliedColor;
                    }
                }
            }
        }

        lastMoved = sysTimeMs();
    }


    size_t deleteRainIdx = 0;
    while (deleteRainIdx < raindropCnt && raindrops[deleteRainIdx].y >= NUM_ROW + trailLength + 1) {
        deleteRainIdx++;
    }

    for (size_t i = deleteRainIdx; i < raindropCnt; i++) {
      raindrops[i-deleteRainIdx] = raindrops[i];
    }
    raindropCnt -= deleteRainIdx;
    

    if (nextRainSpawn <= sysTimeMs()) {
        if (raindropCnt < raindropsBufferSize) {
            raindrops[raindropCnt].x = randInt() % NUM_COLUMN;
            raindrops[raindropCnt].y = 0;

            raindropCnt++;
        }

        //nextRainSpawn = sysTimeMs() + randInt() % 100 + 80;
        nextRainSpawn = sysTimeMs() + randInt() % ((105-rainIntensity) * 3) + 70;
    }
}


////// THUNDER //////

typedef struct {
    uint8_t col;
    int8_t intensity;
    
    uint8_t maxFlashes;
    uint8_t currFlash;
    systime_t timeThreshold;
    bool state;
} lightning;

lightning lightn;

systime_t nextLightnSpawn = 0;

uint8_t stormIntensity = 50;

void storm_init(led_t* ledColors) {
    rain_init(ledColors);
    stormIntensity = 50;
}

void anim_storm(led_t* ledColors) {
    // Call rain animation
    anim_rain(ledColors);

    // Add lightning
    if (sysTimeMs() >= nextLightnSpawn) {
        lightn.col = randInt() % NUM_COLUMN;
        lightn.maxFlashes = randInt() % 6 + 1;
        lightn.currFlash = 0;
        lightn.state = 0;
        lightn.timeThreshold = sysTimeMs();
        
        // nextLightnSpawn = sysTimeMs() + randInt() % 9000 + 2000;
        nextLightnSpawn = sysTimeMs() + randInt() % ((110-stormIntensity) * 180) + 1000;
    }


    lightn.intensity -= 5;
    if (lightn.intensity < 0) {
        lightn.intensity = 0;
    }


    if (lightn.intensity < 10 && lightn.currFlash >= lightn.maxFlashes) {
        lightn.intensity = 0;
    }


    if (lightn.currFlash < lightn.maxFlashes && lightn.timeThreshold <= sysTimeMs()) {
        lightn.currFlash++;
        lightn.state = !lightn.state;
        lightn.intensity = randInt() % 30 + 71;
        
        if (lightn.state) {
            lightn.timeThreshold = sysTimeMs() + randInt() % 700 + 150;
        }
        else {
            lightn.timeThreshold = sysTimeMs() + randInt() % 40 + 30;
        }
    }

    if (lightn.state || (lightn.intensity && lightn.currFlash >= lightn.maxFlashes)) {
        led_t lightnColor = {200, 255, 255};
        multiplyColor(&lightnColor, lightn.intensity, &lightnColor);

        for (uint8_t y = 0; y < NUM_ROW; y++) {
            ledColors[y * NUM_COLUMN + lightn.col] = lightnColor;
        }
    }
}



////// BREATHING //////

typedef struct {
    pos_i pos;
    led_t color;
    int brightness;
} keyTap;

#define keyTapsBufferSize 25
keyTap keyTaps[keyTapsBufferSize];
size_t keyTapsCnt = 0;
const uint8_t fadeSpeed = 8;


void anim_breathing(led_t* ledColors) {
    setAllColors(ledColors, &black);
    for (size_t i = 0; i < keyTapsCnt; i++) {
        multiplyColor(&keyTaps[i].color, keyTaps[i].brightness, 
            &ledColors[keyTaps[i].pos.y * NUM_COLUMN + keyTaps[i].pos.x]);

        keyTaps[i].brightness -= fadeSpeed;
    }


    size_t deleteTapIdx = 0;
    while (deleteTapIdx < keyTapsCnt && keyTaps[deleteTapIdx].brightness <= 0) {
        deleteTapIdx++;
    }

    for (size_t i = deleteTapIdx; i < keyTapsCnt; i++) {
      keyTaps[i-deleteTapIdx] = keyTaps[i];
    }
    keyTapsCnt -= deleteTapIdx;
}

void breathing_init(led_t* ledColors) {
    keyTapsCnt = 0;
}


led_t breathing_colors[] = {
    red,
    green,
    blue,
    pink,
    purple,
    yellow,
    orange,
    turkiz
};

void pressed_breathing(uint8_t x, uint8_t y, led_t* ledColors) {
    if(keyTapsCnt < keyTapsBufferSize) {
        keyTaps[keyTapsCnt].pos.x = x;
        keyTaps[keyTapsCnt].pos.y = y;
        keyTaps[keyTapsCnt].color = 
            breathing_colors[randInt() % LEN(breathing_colors)];
        keyTaps[keyTapsCnt].brightness = 100;

        keyTapsCnt++;
    }
}



////// SNOWING //////

typedef struct {
    pos_i pos;
    int8_t timer;
    uint8_t fallSpeed;
} snowflake;

#define snowflakesBufferSize 30
snowflake snowflakes[snowflakesBufferSize];
uint8_t snowflakeCnt = 0;

int snowflakeSpawnTimer = 0;
uint8_t snowIntensity = 50;

void anim_snowing(led_t* ledColors) {
    setAllColors(ledColors, &black);

    for (int i = 0; i < snowflakeCnt; i++) {
        setColor(ledColors, &snowflakes[i].pos, &white);
        if (--snowflakes[i].timer <= 0) {
            snowflakes[i].pos.y++;
            snowflakes[i].timer = snowflakes[i].fallSpeed;
        }
    }
    

    //// delete snowflakes
    uint8_t deleteIdx = 0;
    while (deleteIdx < snowflakeCnt && snowflakes[deleteIdx].pos.y >= 5) {
        deleteIdx++;
    }

    for (size_t i = deleteIdx; i < snowflakeCnt; i++) {
      snowflakes[i-deleteIdx] = snowflakes[i];
    }
    snowflakeCnt -= deleteIdx;


    //// spawn snowflakes
    if (--snowflakeSpawnTimer <= 0) {
        if (snowflakeCnt < snowflakesBufferSize) {
            snowflakes[snowflakeCnt].pos.x = randInt() % NUM_COLUMN;
            snowflakes[snowflakeCnt].pos.y = -1;
            snowflakes[snowflakeCnt].fallSpeed = randInt() % 3 + 6;
            snowflakes[snowflakeCnt].timer = 0;

            snowflakeCnt++;
        }

        // snowflakeSpawnTimer = randInt() % 4 + 6;
        snowflakeSpawnTimer = randInt() % ((110-snowIntensity) / 10) + 3;
    }
}

void snowing_init(led_t* ledColors) {
    snowflakeCnt = 0;
    snowIntensity = 50;
}


////// LOCKED ////// 

led_t locked_color = {255, 20, 20};

uint8_t lockedIntensity = 0;
bool lockedAnimDir = 1;

void anim_locked(led_t* ledColors) {
    if (lockedAnimDir) {
        lockedIntensity += 2;

        if (lockedIntensity >= 100) {
            lockedAnimDir = 0;
        }
    }
    else {
        lockedIntensity--;

        if (lockedIntensity <= 0) {
            lockedAnimDir = 1;
        }
    }

    led_t multipliedColor;
    multiplyColor(&locked_color, lockedIntensity, &multipliedColor);
    
    setAllColors(ledColors, &multipliedColor);
}

void locked_init(led_t* ledColors) {
    lockedIntensity = 0;
    lockedAnimDir = 1;
}


////// STARS //////

typedef struct {
    pos_i pos;
    uint8_t intensity;
} star;


static const star stars[] = {
    { {1, 0}, 40 },
    { {1, 2}, 100 },
    { {5, 1}, 30 },
    { {11, 3}, 60 },
    { {6, 4}, 100 },
    { {13, 1}, 85 },
    { {8, 0}, 15 },
    { {8, 2}, 50 },
    { {3, 3}, 10 },
};

void anim_stars(led_t* ledColors) {
    setAllColors(ledColors, &black);

    for (uint8_t i = 0; i < LEN(stars); i++) {
        uint8_t intensityMod = randInt() % 60 + 70;
        int intensity = stars[i].intensity * intensityMod / 100;
        if (intensity > 100) 
            intensity = 100;

        multiplyColor(&white, intensity, &ledColors[stars[i].pos.y * NUM_COLUMN + stars[i].pos.x]);
    }
}


#include "math.h"

////// SUNNY //////

static uint8_t angles[] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    90, 40, 37, 34, 31, 28, 25, 22, 19, 17, 15, 13, 11, 10,
    90, 50, 47, 44, 41, 38, 35, 32, 29, 27, 25, 23, 21, 19,
    90, 67, 64, 61, 58, 55, 52, 49, 46, 43, 40, 37, 34, 30,
    90, 77, 74, 71, 68, 65, 62, 59, 56, 53, 50, 47, 44, 40,
};


static uint16_t sunRotation = 0;
static pos_i sunPos = {0, 0};

static led_t sunny_color = yellow;


void anim_sunny(led_t* ledColors) {
    for (uint8_t y = 0; y < NUM_ROW; y++) {
        for (uint8_t x = 0; x < NUM_COLUMN; x++) {
            uint8_t x_rel = abs(x - sunPos.x);
            uint8_t y_rel = abs(y - sunPos.y);
            int angle;
            // if (x_rel != 0)
            //     angle = ((int)atan(y_rel,x_rel) + sunRotation) % 360;
            // else
                // angle = (270 + sunRotation) % 360;

            angle = (angles[y * NUM_COLUMN + x] + sunRotation) % 360;

            uint8_t brightness = (abs((angle % 72) - 36)) * ((int)100) / 36;
            if (brightness > 100)
                brightness = 100;

            multiplyColor(&sunny_color, brightness, &ledColors[y * NUM_COLUMN + x]);
        }
    }

    ledColors[0] = sunny_color;
    
    sunRotation = (sunRotation + 1) % 360;
}

void sunny_init(led_t* ledColors) {
    led_t sunny_color = yellow;
}


////// LIVE WEATHER //////

static bool weatherUpToDate = false;
static WeatherData weatherData;
static void(*weatherAnimFn)(led_t*) = 0;

void setWeatherData(WeatherData* data) {
    weatherUpToDate = true;
    weatherData = *data;

    if (weatherData.snowIntensity > 0) {
        weatherAnimFn = anim_snowing;
        reactiveFps = 30;

        snowing_init(0);
        
        int snowInt = weatherData.stormIntensity * 3;
        if (snowInt > 100)
            snowInt = 100;
        snowIntensity = snowInt;

        int rainInt = weatherData.rainIntensity * 3;
        if (rainInt > 100)
            rainInt = 100;
        rainIntensity = rainInt;
    }
    else if (weatherData.stormIntensity > 0) {
        weatherAnimFn = anim_storm;
        reactiveFps = 30;

        storm_init(0);
        stormIntensity = weatherData.stormIntensity;
    }
    else if (weatherData.rainIntensity > 0) {
        weatherAnimFn = anim_rain;
        reactiveFps = 30;

        rain_init(0);
        int rainInt = weatherData.rainIntensity * 3;
        if (rainInt > 100)
            rainInt = 100;
        rainIntensity = rainInt;
    }
    else if (!(weatherData.time.hour > 6 && weatherData.time.hour < 19)) {
        weatherAnimFn = anim_stars;
        reactiveFps = 6;
    }
    else if (weatherData.cloudDensity >= 50) {
        weatherAnimFn = anim_sunny;
        reactiveFps = 15;

        sunny_init(0);

        const led_t cloudColor = {100, 100, 100};
        sunny_color = cloudColor;
    }
    else {
        weatherAnimFn = anim_sunny;
        reactiveFps = 30;

        sunny_init(0);
    }
}

void anim_liveWeather(led_t* ledColors) {
    if (weatherUpToDate && weatherAnimFn) {
        weatherAnimFn(ledColors);
    }
    else {
        setAllColors(ledColors, &black);
        ledColors[0] = red;
    }
}





