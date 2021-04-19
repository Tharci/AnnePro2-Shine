#include "led_state.h"
#include "ch.h"
#include "led_animation.h"
#include "string.h"
#include "common_utils.h"
#include "profiles.h"


//// State ////

static bool ledState        = false;
static bool ledTimeoutState = true; 
static bool capsState       = false;
static int brightness = 100;
static bool gamingMode = false;
static bool isLocked = false;
static int8_t currentProfile = 0;
static PowerPlan powerPlan = POWER_USB;

static const systime_t numDisplaySpeed  = 400;
static int8_t numToDisplay[20]; // numbers in the range of 0-9, number '-1' indicates '-' character
static int numToDisplayIdx = -1;
static bool numDisplayOn = false;
static systime_t numDisplayLastSwitched = 0;
static led_t numDisplayColor = {200, 255, 255};

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

static const Profile profiles[] = {
    { 30, animatedRainbowFlow, 0, 0 },
    { 30, prof_breathing_tick, prof_breathing_init, prof_breathing_pressed },
    { REACTIVE_FPS,  prof_liveWeather_tick, prof_liveWeather_init, 0 },
    { 30, prof_blink_tick, prof_blink_init, 0 },
    { REACTIVE_FPS, prof_weatherShowoff_tick, prof_weatherShowoff_init, 0 }
};

static const Profile lockedProfile = { 30, prof_locked_tick, prof_locked_init, 0 };
static const uint8_t profileCount = sizeof(profiles)/sizeof(Profile);

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


//// One-Shot Effect ////

static led_t oneShotLedColors[70];

OneShotEffect oneShotEffect = {
    .tick = NULL
};

void registerOneShotEffect(OneShotEffect effect) {
    oneShotEffect = effect;
    if (oneShotEffect.init) {
        oneShotEffect.init(oneShotLedColors);
    }

    memset(oneShotLedColors, 0, NUM_COLUMN * NUM_ROW * sizeof(led_t));
}

bool oneShotEffectIsActive() {
    return oneShotEffect.tick != NULL;
}

void disableOneShotEffect() {
    oneShotEffect.tick = NULL;
}

OneShotEffect* getOneShotEffect() {
    return &oneShotEffect;
}

void executeOneShotEffect(void) {
    if (oneShotEffectIsActive()) {
        if (!oneShotEffect.tick(oneShotLedColors)) {
            disableOneShotEffect();
        }
    }
}

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
    if (powerPlan == POWER_MAX) {
        ledTimeoutState = true;
    }
    else if (ledTimeoutState) {
        int ledTimeout = (powerPlan == POWER_BATT) ?
            LED_TIMEOUT_BATTERY : LED_TIMEOUT_USB;

        if (sysTimeMs() - lastKeypress >= ledTimeout * 1000) {
            ledTimeoutState = false;
            memset(ledColors, 0, NUM_COLUMN * NUM_ROW * sizeof(led_t));
        }
    }
}

void ledPostProcess() {
    if(ledState && ledTimeoutState && numToDisplayIdx < 0) {
        memcpy(ledColorsPost, ledColors, NUM_COLUMN * NUM_ROW * sizeof(led_t));
    }
    else {
        memset(ledColorsPost, 0, NUM_COLUMN * NUM_ROW * sizeof(led_t));
    }


    if (!isLocked && !oneShotEffectIsActive()) {
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


        if (numToDisplayIdx >= 0) {
            if (sysTimeMs() - numDisplayLastSwitched > numDisplaySpeed) {
                numDisplayOn = !numDisplayOn;
                numDisplayLastSwitched = sysTimeMs();

                if (numDisplayOn == false) {
                    numToDisplayIdx--;
                }
            }


            if (numDisplayOn) {
                int8_t digitIdx = numToDisplay[numToDisplayIdx];

                if (digitIdx == 0) 
                    digitIdx = 10;
                else if (digitIdx == -1)
                    digitIdx = 11;
                else if (digitIdx < -1 || digitIdx > 9)
                    digitIdx = 0;

                ledColorsPost[digitIdx] = numDisplayColor;
            }
        }
    }

    if (oneShotEffectIsActive()) {
        memcpy(ledFinal, oneShotLedColors, NUM_COLUMN * NUM_ROW * sizeof(led_t));
    }
    else {
        memcpy(ledFinal, ledColorsPost, NUM_COLUMN * NUM_ROW * sizeof(led_t));
    }
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


static OneShotEffect powerBattEffect = {
    effect_power_batt_init,
    effect_power_tick,
    60
};

static OneShotEffect powerUsbEffect = {
    effect_power_usb_init,
    effect_power_tick,
    60
};

static OneShotEffect powerMaxEffect = {
    effect_power_max_init,
    effect_power_tick,
    60
};

void setPowerPlan(PowerPlan pp) {
    powerPlan = pp;

    switch (powerPlan)
    {
    case POWER_BATT:
        registerOneShotEffect(powerBattEffect);
        break;
        
    case POWER_USB:
        registerOneShotEffect(powerUsbEffect);
        break;
        
    case POWER_MAX:
        registerOneShotEffect(powerMaxEffect);
        break;
    
    default:
        break;
    }
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


static void displayNumberWhite(int value) {
    numDisplayColor.red     = 0;
    numDisplayColor.green   = 0;
    numDisplayColor.blue    = 0;

    displayNumber(value);
}

void displayNumber(int value) {
    bool isNegative = (value < 0);
    value = abs(value);

    numToDisplayIdx = 0;
    do {
        numToDisplay[numToDisplayIdx++] = value % 10;
        value /= 10;
    } while (value > 0);

    if (isNegative)
        numToDisplay[numToDisplayIdx++] = -1;

    numDisplayOn = true;
}

void displayNumberColored(int value, led_t color) {
    numDisplayColor = color;
    displayNumber(value);
}


static void displayInvalidNumber(void) {
    numToDisplayIdx = 0;
    numToDisplay[0] = -10;
}

void displayTemp(void) {
    if (weatherIsUpToDate()) {
        led_t yellow = {180, 255, 0};
        displayNumberColored(getWeatherData()->temp, yellow);
    }
    else {
        displayInvalidNumber();
    }
}

void displayTime(void) {
    Time currTime = getCurrentTime();

    numToDisplay[3] = currTime.hour / 10;
    numToDisplay[2] = currTime.hour % 10;
    numToDisplay[1] = currTime.minute / 10;
    numToDisplay[0] = currTime.minute % 10;

    led_t green = {0, 255, 30};
    numDisplayColor = green;
    
    numToDisplayIdx = 3;
}

