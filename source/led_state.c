#include "led_state.h"
#include "ch.h"
#include "led_animation.h"
#include "string.h"
#include "common_utils.h"


//// State ////

static bool ledState        = false;
static bool ledTimeoutState = true; 
static bool capsState       = false;
static int brightness = 100;
static bool gamingMode = false;
static bool isLocked = false;
static int8_t currentProfile = 0;
PowerPlan powerPlan = POWER_USB;

#define LED_TIMEOUT_BATTERY 180
#define LED_TIMEOUT_USB     1200
static systime_t lastKeypress;

/* bluetooth indicator */
static const systime_t bltConnBlinkSpeed  = 500;
static const systime_t bltBroadBlinkSpeed = 250;
// 0 means that it is not connecting
static uint8_t bltState = 0; 
static bool bltLedOn = false;
static systime_t bltLedLastSwitched = 0;
/* */

//// ////


//// Profiles ////

static Profile profiles[] = {
    { 30, anim_rain, rain_init, 0 },
    { 30, anim_storm, storm_init, 0 },
    { 30, animatedRainbowFlow, 0, 0 },
    { 30, anim_breathing, breathing_init, pressed_breathing },
    { 30, anim_snowing, snowing_init, 0 },
    { 6,  anim_stars, 0, 0 },
    { 30, anim_sunny, sunny_init, 0 },
    { REACTIVE_FPS,  anim_liveWeather, liveWeather_init, 0 },
};

static Profile lockedProfile = { 30, anim_locked, locked_init, 0 };
static uint8_t profileCount = sizeof(profiles)/sizeof(Profile);

//// ////


//// Keypress ////

typedef struct {
  uint8_t col;
  uint8_t row;
} keypress;

#define pressedKeysBuffSize 35
static keypress pressedKeys[pressedKeysBuffSize];
static uint8_t pressedKeyCnt = 0;

//// ////


//// Led Maps ////

static led_t ledColorsPost[70];
static led_t ledFinal[70];
static led_t ledColors[70];

//// ////

Profile* getCurrentProfile() {
    if (isLocked)
        return &lockedProfile;
    else
        return &profiles[currentProfile];
}

uint8_t getCurrentProfileIndex() {
    return currentProfile;
}




void switchProfile(int profile) {
    currentProfile = profile % getProfileCount();
    executeInit();
}

void nextProfile() {
    switchProfile(currentProfile + 1);
}

void prevProfile() {
    switchProfile(currentProfile - 1);
}

void disableLeds() {
    ledState = false;
}

void enableLeds(){
    ledState = true;
}


void toggleLeds() {
    if (ledState) {
        disableLeds();
    }
    else {
        enableLeds();
    }
}

void updateTimeout() {
    if (ledTimeoutState) {
        int ledTimeout = (powerPlan == POWER_BATTERY) ?
            LED_TIMEOUT_BATTERY : LED_TIMEOUT_USB;

        if (sysTimeMs() - lastKeypress >= ledTimeout * 1000) {
            ledTimeoutState = false;
            memset(ledColors, 0, NUM_COLUMN * NUM_ROW * sizeof(led_t));
        }
    }
}

void ledPostProcess() {
    if(ledState && ledTimeoutState) {
        memcpy(ledColorsPost, ledColors, NUM_COLUMN * NUM_ROW * sizeof(led_t));
    }
    else {
        memset(ledColorsPost, 0, NUM_COLUMN * NUM_ROW * sizeof(led_t));
    }


    if (!isLocked) {
        if (brightness < 100) {
            for (int i = 0; i < NUM_COLUMN * NUM_ROW; i++) {
                ledColorsPost[i].red = (ledColorsPost[i].red * brightness) / 100;
                ledColorsPost[i].green = (ledColorsPost[i].green * brightness) / 100;
                ledColorsPost[i].blue = (ledColorsPost[i].blue * brightness) / 100;
            }
        }


        static const led_t capsColor = { 255, 25, 25 };
        if (capsState) {
            ledColorsPost[28] = capsColor;
        }


        static const led_t bltColor = {0, 255, 30};

        if (bltState) {
            systime_t bltLedRate = bltState <= 4 ? bltConnBlinkSpeed : bltBroadBlinkSpeed;

            if (sysTimeMs() - bltLedLastSwitched > bltLedRate) {
                bltLedOn = !bltLedOn;
                bltLedLastSwitched = sysTimeMs();
            }

            if (bltLedOn)
                ledColorsPost[(bltState - 1) % 4 + 1] = bltColor;
        }


        static const uint8_t gamingArrows[] = { 54, 66, 67, 68 };
        static const led_t gamingArrowLedColor = { 110, 5, 5 };
        if (gamingMode) {
            for (uint8_t i = 0; i < LEN(gamingArrows); i++) {
                ledColorsPost[gamingArrows[i]] = gamingArrowLedColor;
            }
        }
    }

    memcpy(ledFinal, ledColorsPost, NUM_COLUMN * NUM_ROW * sizeof(led_t));
}


void led_state_init() {
    lastKeypress = sysTimeMs();
    memset(ledColors, 0, NUM_COLUMN * NUM_ROW * sizeof(led_t));
}


void bltConnected() {
    bltState = 0;
}

void bltConnecting(uint8_t state) {
    bltState = state;

    bltLedOn = true;
    bltLedLastSwitched = sysTimeMs();
}

void brightnessDown() {
    brightness -= 20;
    if (brightness < 10)
        brightness = 10;
}

void brightnessUp() {
    brightness += 20;
    if (brightness > 100)
        brightness = 100;
}

void setBrightness(uint8_t bn) {
    brightness = bn;
}

void setGamingMode(bool mode) {
    gamingMode = mode;
}

void setCapsState(bool state) {
    capsState = state;
}

int8_t getBrightness(void) {
    return brightness;
}


void keyPressedCallback(uint8_t keyPos) {
    lastKeypress = sysTimeMs();

    if (ledTimeoutState == false) {
        executeInit();
        ledTimeoutState = true;
    }

    if (pressedKeyCnt < pressedKeysBuffSize) {
        pressedKeys[pressedKeyCnt].row = keyPos & 0xF;
        pressedKeys[pressedKeyCnt].col = (keyPos >> 4) & 0xF; 
        pressedKeyCnt++;
    }
}


void setPowerPlan(PowerPlan pp) {
    powerPlan = pp;
}

void setLocked(bool locked) {
    isLocked = locked;
}

uint8_t getProfileCount(void) {
    return profileCount;
}



void executeProfile() {
    if (ledState && ledTimeoutState) {
        anim_tick tick = getCurrentProfile()->tick;
        if (tick) tick(ledColors);
    }

    executeKeypress();
    ledPostProcess();
}



void executeInit() {
    memset(ledColors, 0, NUM_COLUMN * NUM_ROW * sizeof(led_t));
    anim_init init = getCurrentProfile()->init;
    if (init) {
        init(ledColors);
    }
}

void executeKeypress() {
    anim_keypress callback = getCurrentProfile()->keypress;
    if (callback) {
        for (uint8_t i = 0; i < pressedKeyCnt; i++) {
            callback(pressedKeys[i].col, pressedKeys[i].row, ledColors);
        }
    }

    pressedKeyCnt = 0;
}


led_t* getLedsToDisplay() {
    return ledFinal;
}
