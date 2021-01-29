#include "extra_profiles.h"
#include "miniFastLED.h"
#include "stdlib.h"
#include "ap2_qmk_led.h"



////// LIGHT UTILS //////

const led_t black   = {0, 0, 0};
const led_t white   = {200, 255, 255};
const led_t red     = {255, 0, 0};
const led_t green   = {0, 255, 0};
const led_t blue    = {0, 0, 255};
const led_t pink    = {200, 0, 255};
const led_t purple  = {75, 0, 255};
const led_t yellow  = {180, 255, 0};
const led_t orange  = {255, 140, 0};
const led_t turkiz  = {0, 255, 255};



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



////// ANIMATED RAIN //////

const led_t rainBg = {0, 0, 0};
const led_t rainColor = {10, 10, 255};

#define raindropsBufferSize 40

static uint8_t raindropCnt = 0;
static pos_i raindrops[raindropsBufferSize];

systime_t nextRainSpawn = 0;
static const uint8_t trailLength = 6;

systime_t rainSpeedMs = 55;
systime_t lastMoved = 0;

void anim_rain(led_t* ledColors) {
    if (sysTimeMs() - lastMoved > rainSpeedMs) {
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

        nextRainSpawn = sysTimeMs() + randInt() % 100 + 50;
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
    if (sysTimeMs() >= nextLightnSpawn) {
        lightn.col = randInt() % NUM_COLUMN;
        lightn.maxFlashes = randInt() % 6 + 1;
        lightn.currFlash = 0;
        lightn.state = 0;
        lightn.timeThreshold = sysTimeMs();
        
        nextLightnSpawn = sysTimeMs() + randInt() % 9000 + 2000;
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


led_t breathing_colors[] = {
    red,
    green,
    blue,
    pink,
    purple,
    yellow,
    orange,
    turkiz,
    white
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


////// LOCKED ////// 

led_t locked_color = {255, 20, 20};

uint8_t locked_intensity = 0;
bool locked_anim_dir = 1;

void anim_locked(led_t* ledColors) {
    if (locked_anim_dir) {
        locked_intensity++;

        if (locked_intensity >= 100) {
            locked_anim_dir = 0;
        }
    }
    else {
        locked_intensity--;

        if (locked_intensity <= 0) {
            locked_anim_dir = 1;
        }
    }

    led_t multipliedColor;
    multiplyColor(&locked_color, locked_intensity, &multipliedColor);
    
    setAllColors(ledColors, &multipliedColor);
}




