#include "extra_profiles.h"
#include "miniFastLED.h"
#include "stdlib.h"



////// RANDOM UTILS //////
typedef struct {
    int8_t x, y;
} pos_i;



static unsigned long rand_x=123456789, rand_y=362436069, rand_z=521288629;

unsigned long randInt(void) {
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


void setAllColors(led_t* ledColors, const led_t* color) {
    for (int i = 0; i < NUM_ROW * NUM_COLUMN; i++) {
        ledColors[i] = *color;
    }
}

bool legitPosition(const pos_i* pos) {
    return
        pos->x >= 0 && pos->x < NUM_COLUMN &&
        pos->y >= 0 && pos->y < NUM_ROW;
}

void setColor(led_t* ledColors, const pos_i* pos, const led_t* color) {
    if (legitPosition(pos))
        ledColors[pos->y * NUM_COLUMN + pos->x] = *color;
}


void multiplyColor(const led_t* color, uint8_t brightness, led_t* color2) {
    color2->red   = (unsigned char)(color->red * brightness / 100);
    color2->green = (unsigned char)(color->green * brightness / 100);
    color2->blue  = (unsigned char)(color->blue * brightness / 100);
}

const led_t black = {0, 0, 0};
const led_t white = {200, 255, 255};



////// ANIMATED RAIN //////

const led_t rainBg = {0, 0, 0};
const led_t rainColor = {10, 10, 255};

#define sysTime chVTGetSystemTime

#define raindropsBufferSize 40
// const unsigned char raindropsBufferSize = 40;
static uint8_t raindropCnt = 0;
static pos_i raindrops[raindropsBufferSize];

systime_t nextRainSpawn = 0;
static const uint8_t trailLength = 6;

systime_t rainSpeedMs = 55;
systime_t lastMoved = 0;

void anim_rain(led_t* ledColors) {
    if (sysTime() - lastMoved > rainSpeedMs * 10) {
        for (size_t i = 0; i < raindropCnt; i++) {
            raindrops[i].y++;
        }
        
        setAllColors(ledColors, &rainBg);
        for (uint8_t i = 0; i < raindropCnt; i++) {
            for (uint8_t y = 0; y < NUM_ROW && y <= raindrops[i].y; y++) {
                if (raindrops[i].y - y < trailLength + 1) {
                    uint8_t brightness = 100 - ((raindrops[i].y - y) * 100 / trailLength);

                    pos_i currPos = { raindrops[i].x, y };
                    led_t multipliedColor;
                    multiplyColor(&rainColor, brightness, &multipliedColor);
                    setColor(ledColors, &currPos, &multipliedColor);
                }
            }
        }

        lastMoved = sysTime();
    }


    size_t deleteRainIdx = 0;
    while (deleteRainIdx < raindropCnt && raindrops[deleteRainIdx].y >= NUM_ROW + trailLength + 1) {
        deleteRainIdx++;
    }

    for (size_t i = deleteRainIdx; i < raindropCnt; i++) {
      raindrops[i-deleteRainIdx] = raindrops[i];
    }
    raindropCnt -= deleteRainIdx;
    

    if (nextRainSpawn <= sysTime()) {
        if (raindropCnt < raindropsBufferSize) {
            raindrops[raindropCnt].x = randInt() % NUM_COLUMN;
            raindrops[raindropCnt].y = 0;

            raindropCnt++;
        }

        nextRainSpawn = sysTime() + randInt() % 1000 + 500;
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


void anim_thunder(led_t* ledColors) {
    // Call rain animation
    anim_rain(ledColors);

    // Add lightning
    if (sysTime() >= nextLightnSpawn) {
        lightn.col = randInt() % NUM_COLUMN;
        lightn.maxFlashes = randInt() % 6 + 1;
        lightn.currFlash = 0;
        lightn.state = 0;
        lightn.timeThreshold = sysTime();
        
        nextLightnSpawn = sysTime() + randInt() % 100000 + 30000;
    }


    lightn.intensity -= 7;
    if (lightn.intensity < 0) {
        lightn.intensity = 0;
    }


    if (lightn.intensity < 10 && lightn.currFlash >= lightn.maxFlashes) {
        lightn.intensity = 0;
    }


    if (lightn.currFlash < lightn.maxFlashes && lightn.timeThreshold <= sysTime()) {
        lightn.currFlash++;
        lightn.state = !lightn.state;
        lightn.intensity = randInt() % 30 + 71;
        
        if (lightn.state) {
            lightn.timeThreshold = sysTime() + randInt() % 7000 + 1500;
        }
        else {
            lightn.timeThreshold = sysTime() + randInt() % 200 + 50;
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
        led_t multipliedColor;
        multiplyColor(&keyTaps[i].color, keyTaps[i].brightness, &multipliedColor);
        setColor(ledColors, &keyTaps[i].pos, &multipliedColor);

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

void pressed_breathing(uint8_t x, uint8_t y, led_t* ledColors) {
    if(keyTapsCnt < keyTapsBufferSize) {
        keyTaps[keyTapsCnt].pos.x = x;
        keyTaps[keyTapsCnt].pos.y = y;
        keyTaps[keyTapsCnt].color.red = randInt()%256;
        keyTaps[keyTapsCnt].color.green = randInt()%256;
        keyTaps[keyTapsCnt].color.blue = randInt()%256;
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

        snowflakeSpawnTimer = randInt() % 4 + 6;
    }
}



