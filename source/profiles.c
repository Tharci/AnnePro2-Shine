#include "profiles.h"
#include "stdlib.h"
#include "common_utils.h"



////// LIGHT UTILS //////

typedef struct {
    int8_t x, y;
} pos_i;

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



////// RAINBOW FLOW //////

static uint8_t flowValue[NUM_COLUMN] = {
  0, 11, 22, 33, 44, 55, 66, 77,
  88, 99, 110, 121, 132, 143
};
void animatedRainbowFlow(led_t* ledColors){
  for(int i = 0; i < NUM_COLUMN; i++){
    setColumnColorHSV(ledColors, i, flowValue[i], 255, 125);
    if(flowValue[i] == 179){
      flowValue[i] = 240;
    }
    flowValue[i]++;
  }
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

void prof_rain_init(led_t* ledColors) {
    raindropCnt = 0;
    nextRainSpawn = 0;
    lastMoved = 0;
    rainIntensity = 50;
}

void prof_rain_tick(led_t* ledColors) {
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

        // range of the intensity multiplier is : 50% - 300%
        nextRainSpawn = sysTimeMs() + (randInt() % 50 + 30) * (120 - rainIntensity) * 5 / 200;
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

void prof_storm_init(led_t* ledColors) {
    prof_rain_init(ledColors);
    stormIntensity = 50;
}

void prof_storm_tick(led_t* ledColors) {
    // Call rain animation
    prof_rain_tick(ledColors);

    // Add lightning
    if (sysTimeMs() >= nextLightnSpawn) {
        lightn.col = randInt() % NUM_COLUMN;
        lightn.maxFlashes = randInt() % 6 + 1;
        lightn.currFlash = 0;
        lightn.state = 0;
        lightn.timeThreshold = sysTimeMs();

        // range of the intensity multiplier is : 50% - 250%
        nextLightnSpawn = sysTimeMs() + (randInt() % 9000 + 2000) * (125 - stormIntensity) / 50;
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
} breathKeypress;

#define breathKeypressBuffSize 25
breathKeypress breathKeypresses[breathKeypressBuffSize];
size_t breathKeypressCount = 0;
const uint8_t breathFadeSpeed = 8;


void prof_breathing_tick(led_t* ledColors) {
    setAllColors(ledColors, &black);
    for (size_t i = 0; i < breathKeypressCount; i++) {
        multiplyColor(&breathKeypresses[i].color, breathKeypresses[i].brightness, 
            &ledColors[breathKeypresses[i].pos.y * NUM_COLUMN + breathKeypresses[i].pos.x]);

        breathKeypresses[i].brightness -= breathFadeSpeed;
    }


    size_t deleteTapIdx = 0;
    while (deleteTapIdx < breathKeypressCount && breathKeypresses[deleteTapIdx].brightness <= 0) {
        deleteTapIdx++;
    }

    for (size_t i = deleteTapIdx; i < breathKeypressCount; i++) {
      breathKeypresses[i-deleteTapIdx] = breathKeypresses[i];
    }
    breathKeypressCount -= deleteTapIdx;
}

void prof_breathing_init(led_t* ledColors) {
    breathKeypressCount = 0;
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

void prof_breathing_pressed(uint8_t x, uint8_t y, led_t* ledColors) {
    if(breathKeypressCount < breathKeypressBuffSize) {
        breathKeypresses[breathKeypressCount].pos.x = x;
        breathKeypresses[breathKeypressCount].pos.y = y;
        breathKeypresses[breathKeypressCount].color = 
            breathing_colors[randInt() % LEN(breathing_colors)];
        breathKeypresses[breathKeypressCount].brightness = 100;

        breathKeypressCount++;
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

void prof_snowing_tick(led_t* ledColors) {
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
        snowflakeSpawnTimer = randInt() % ((105-snowIntensity) / 3) + 3;
    }
}

void prof_snowing_init(led_t* ledColors) {
    snowflakeCnt = 0;
    snowIntensity = 50;
}


////// LOCKED ////// 

led_t locked_color = {255, 20, 20};

uint8_t lockedIntensity = 0;
bool lockedAnimDir = 1;

void prof_locked_tick(led_t* ledColors) {
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

void prof_locked_init(led_t* ledColors) {
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

void prof_stars_tick(led_t* ledColors) {
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


void prof_sunny_tick(led_t* ledColors) {
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

            brightness = (int)brightness /* * sunIntensity / 100*/;

            multiplyColor(&yellow, brightness, &ledColors[y * NUM_COLUMN + x]);
        }
    }

    ledColors[0] = yellow;
    
    sunRotation = (sunRotation + 1) % 360;
}

void prof_sunny_init(led_t* ledColors) {
    
}


////// CLOUDY ///////

static uint8_t sin_lookup[360] = {
    0, 1, 3, 5, 6, 8, 10, 12, 13, 15, 17, 19, 20, 22, 24, 25, 27, 29, 30, 32, 34, 35, 37, 39, 40, 42, 43, 45, 46, 48, 49, 51, 52, 54, 55, 57, 58, 60, 61, 62, 64, 65, 66, 68, 69, 70, 71, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 89, 90, 91, 92, 92, 93, 93, 94, 95, 95, 96, 96, 97, 97, 97, 98, 98, 98, 99, 99, 99, 99, 99, 99, 99, 99, 100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 97, 97, 97, 96, 96, 95, 95, 94, 93, 93, 92, 92, 91, 90, 89, 89, 88, 87, 86, 85, 84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73, 71, 70, 69, 68, 66, 65, 64, 62, 61, 60, 58, 57, 55, 54, 52, 51, 49, 48, 46, 45, 43, 42, 40, 39, 37, 35, 34, 32, 30, 29, 27, 25, 24, 22, 20, 19, 17, 15, 13, 12, 10, 8, 6, 5, 3, 1
};


static uint8_t cloudDensity = 100;
static uint16_t cloudPeriod = 0;
static led_t cloudColor = {130, 200, 200};

void prof_cloudy_init(led_t* ledColors) {
    prof_sunny_init(ledColors);

    cloudDensity = 100; 
}

void prof_cloudy_tick(led_t* ledColors) {
    uint8_t maxRow = NUM_ROW * (cloudDensity + 12) / 100;
    
    if (maxRow < NUM_ROW) {
        prof_sunny_tick(ledColors);
    }

    if (cloudDensity) {
        for (uint8_t x = 0; x < NUM_COLUMN; x++) {
            uint8_t brightness = sin_lookup[(cloudPeriod + x * 6) % 180];

            for (uint8_t y = 0; y < maxRow; y++) {
                multiplyColor(&cloudColor, (int)brightness * (maxRow-y) * 20 / 100, &ledColors[y * NUM_COLUMN + x]);
            }
        }

        cloudPeriod = (cloudPeriod + 1) % 180;
    }
}


////// LIVE WEATHER //////

static systime_t weatherLastUpdated = 0;
static bool weatherUpToDate = false;
static WeatherData weatherData;
static void(*weatherAnimFn)(led_t*) = 0;

void setWeatherData(WeatherData* data) {
    weatherData = *data;
    weatherLastUpdated = sysTimeS();
}

void prof_liveWeather_init(led_t* ledColors) {
    void(*prevAnim)(led_t*) = weatherAnimFn;

    if (weatherData.snowIntensity > 0) {
        weatherAnimFn = prof_snowing_tick;
        reactiveFps = 30;

        if (prevAnim != weatherAnimFn)
            prof_snowing_init(ledColors);
    }
    else if (weatherData.stormIntensity > 0) {
        weatherAnimFn = prof_storm_tick;
        reactiveFps = 30;

        if (prevAnim != weatherAnimFn)
            prof_storm_init(ledColors);
    }
    else if (weatherData.rainIntensity > 0) {
        weatherAnimFn = prof_rain_tick;
        reactiveFps = 30;

        if (prevAnim != weatherAnimFn)
            prof_rain_init(ledColors);
    }
    else if (!(
        weatherData.time.hour*60 + weatherData.time.minute > weatherData.sunriseTime.hour*60 + weatherData.sunriseTime.minute &&
        weatherData.time.hour*60 + weatherData.time.minute < weatherData.sunsetTime.hour*60 + weatherData.sunsetTime.minute)
        ) {
        weatherAnimFn = prof_stars_tick;
        reactiveFps = 6;
    }
    else {
        weatherAnimFn = prof_cloudy_tick;
        reactiveFps = 30;

        if (prevAnim != weatherAnimFn)
            prof_cloudy_init(ledColors);
    }
    
    snowIntensity = weatherData.snowIntensity;
    rainIntensity = weatherData.rainIntensity;
    cloudDensity = weatherData.cloudDensity;
}

void prof_liveWeather_tick(led_t* ledColors) {
    weatherUpToDate = sysTimeS() - weatherLastUpdated < 650;

    if (weatherUpToDate && weatherAnimFn) {
        weatherAnimFn(ledColors);
    }
    else {
        setAllColors(ledColors, &black);
        ledColors[16] = yellow;
    }
}


WeatherData* getWeatherData(void) {
    return &weatherData;
}

bool weatherIsUpToDate(void) {
    return weatherUpToDate;
}

Time getCurrentTime() {
    if (weatherUpToDate) {
        Time time = weatherData.time;
        systime_t deltaTime = sysTimeS() - weatherLastUpdated;
        systime_t currTimeS = time.second + time.minute * 60 + time.hour * 3600 + deltaTime;

        time.second = currTimeS % 60;
        time.minute = (currTimeS / 60) % 60;
        time.hour   = (currTimeS / 3600) % 24;
        
        return time;
    }

    Time time = {0, 0, 0};
    return time;
}



////// BLINKING //////

typedef struct {
    pos_i pos;
    led_t color;
    int brightness;
} blink;

#define blinkBuffSize 25
blink blinks[blinkBuffSize];
size_t blinkCount = 0;
const uint8_t blinkFadeSpeed = 8;
systime_t nextBlinkTime = 0;


void prof_blink_init(led_t* ledColors) {
    blinkCount = 0;
}

void prof_blink_tick(led_t* ledColors) {
    systime_t currTimeMs = sysTimeMs();
    if (nextBlinkTime <= currTimeMs) {
        if (blinkCount < blinkBuffSize) {
            blinkCount++;
            //// TODO
        }

        nextBlinkTime = currTimeMs + 300 + randInt() % 600;
    }

}


void prof_blink_pressed(uint8_t x, uint8_t y, led_t* keyColors) {

}




