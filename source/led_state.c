#include "led_state.h"
#include "ch.h"
#include "led_animation.h"
#include "string.h"
#include "common_utils.h"
#include "profiles.h"



//// State ////

static bool ledsNeedUpdate  = false;
static bool ledState        = false;
static bool ledTimeoutState = true; 
static bool capsState       = false;
static int brightness = 100;
static bool gamingMode = false;
static bool isLocked = false;
static int8_t currentProfile = 0;
static PowerPlan powerPlan = POWER_USB;
static bool mainInitDone = false;

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

//// ANIMATION ////

#define ANIMATION_TIMER_FREQUENCY   60
#define FPS_TO_TIMEOUT(X) (ANIMATION_TIMER_FREQUENCY/X)

static void animationCallback(GPTDriver* driver);


// Lighting animation refresh timer
static const GPTConfig lightAnimationConfig = {
    .frequency = ANIMATION_TIMER_FREQUENCY,
    .callback = animationCallback
};


static void animationCallback(GPTDriver* _driver) {
    (void)_driver;
    static uint64_t tickCount = 0;

    updateTimeout();

    executeOverlapEffect(tickCount);
    executeOverlapEffects(tickCount);
    executeProfile(tickCount);

    if (ledsNeedUpdate)
        ledPostProcess();

    ledsNeedUpdate = false;

    tickCount++;
}


void led_anim_init() {
    led_state_init();

    executeInit();
    executeProfile();

    gptStart(&GPTD_BFTM1, &lightAnimationConfig);
    gptStartContinuous(&GPTD_BFTM1, 1);

    // TODO: Register Startup Overlay Effect
}
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


//// Effects ////

typedef enum {
    EFF_WEAVE_GREEN = 0,
    EFF_WEAVE_YELLOW,
    EFF_WEAVE_RED,
} EffectEnum;

static const Effect effects[] = {
    [EFF_WEAVE_GREEN]  = { effect_weave_green_init, effect_weave_tick, 60 },
    [EFF_WEAVE_YELLOW] = { effect_weave_yellow_init, effect_weave_tick, 60 },
    [EFF_WEAVE_RED]    = { effect_weave_red_init, effect_weave_tick, 60 },
};

Effect* getEffect(EffectEnum effect) {
    return &effects[effect];
}

//// ////


//// Overlap Effect ////
/*
 * Only one overlap effect is stored at a time.
 * While an overlap effect is active, it overlaps 
 * the active animation and overlay effects.
 * Status indicators do not get overlapped.
 */

static led_t oneShotLedColors[70];

Effect* overlapEffect = NULL;

void registerOverlapEffect(Effect* effect) {
    if (effect) {
        memset(oneShotLedColors, 0, NUM_COLUMN * NUM_ROW * sizeof(led_t));

        overlapEffect = effect;
        if (overlapEffect->init) {
            overlapEffect->init(oneShotLedColors);
        }
    }
}

bool overlapEffectIsActive() {
    return overlapEffect != NULL;
}

void disableOverlapEffect() {
    overlapEffect = NULL;
}

Effect* getOverlapEffect() {
    return overlapEffect;
}

void executeOverlapEffect(unsigned long tickCount) {
    if (overlapEffectIsActive() && tickCount % FPS_TO_TIMEOUT(overlapEffect->fps) == 0) {
        if (!overlapEffect->tick(oneShotLedColors)) {
            disableOverlapEffect();
        }

        ledsNeedUpdate = true;
    }
}

//// ////


//// Overlay Effect ////
/*
 * Overlay effect is an one-shot animation that is rendered
 * on top of the profile layer and under the status indicator layer.
 */

#define overlayEffectsBufferSize 10
Effect* overlayEffects[overlayEffectsBufferSize];
static int overlayEffectCount = 0;

static led_t overlayLedColors[70];

void registerOverlayEffect(Effect* effect) {
    if (overlayEffectCount < overlayEffectsBufferSize) {
        overlayEffects[overlayEffectCount++] = effect;
        if (effect->init) {
            effect->init(overlayLedColors);
        }
    }
}

void executeOverlayEffects(unsigned long tickCount) {
    memset(overlayLedColors, 0, NUM_COLUMN * NUM_ROW * sizeof(led_t));

    for (int i = 0; i < overlayEffectCount; i++) {
        if (tickCount % FPS_TO_TIMEOUT(overlayEffects[i]->fps) == 0) {
            if (!overlayEffects[i]->tick(ledColorsPost)) {
                for (int j = i; j < overlayEffectCount - 1; j++) {
                    overlayEffects[j] = overlayEffects[j+1];
                }

                i--;
                overlayEffectCount--;
            }

            for(int y = 0; y < NUM_ROW; y++) {
                for (int x = 0; x < NUM_COLUMN; x++) {
                    int idx = y * NUM_COLUMN + x;
                    if (overlayLedColors[idx].red == 0 && overlayLedColors[idx].green == 0 && overlayLedColors[idx].blue == 0) {
                        overlayLedColors[idx] = ledColorsPost[idx];
                        ledsNeedUpdate = true;
                    }
                }
            }
        }
    }
}

//// ////


void mainInitDoneCallback(void) {
    mainInitDone = true;
}

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
    if (overlapEffectIsActive()) {
        memcpy(ledColorsPost, oneShotLedColors, NUM_COLUMN * NUM_ROW * sizeof(led_t));
    }
    else if(ledState && ledTimeoutState && numToDisplayIdx < 0) {
        memcpy(ledColorsPost, ledColors, NUM_COLUMN * NUM_ROW * sizeof(led_t));
    }
    else {
        memset(ledColorsPost, 0, NUM_COLUMN * NUM_ROW * sizeof(led_t));
    }


    if (!isLocked) {
        for (int i = 0; i < NUM_COLUMN * NUM_ROW; i++) {
            if (!(overlayLedColors[i].red == 0 && overlayLedColors[i].green == 0 && overlayLedColors[i].blue == 0)) {
                ledColorsPost[i] = overlayLedColors[i];
            }
        }


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
    if (brightness < 20)
        brightness = 20;
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

int16_t getBrightness(void) {
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

    if (mainInitDone) {
        switch (powerPlan)
        {
        case POWER_BATT:
            registerOverlayEffect(getEffect(EFF_WEAVE_GREEN));
            break;
            
        case POWER_USB:
            registerOverlayEffect(getEffect(EFF_WEAVE_YELLOW));
            break;
            
        case POWER_MAX:
            registerOverlayEffect(getEffect(EFF_WEAVE_RED));
            break;
        
        default:
            break;
        }
    }
}

void setLocked(bool locked) {
    isLocked = locked;
}

uint8_t getProfileCount(void) {
    return profileCount;
}

void executeProfile(unsigned long tickCount) {
    uint8_t profileFps = getCurrentProfile()->fps;
    if (profileFps == REACTIVE_FPS) {
        profileFps = getReactiveFps();
    }
    
    if (tickCount % FPS_TO_TIMEOUT(profileFps) == 0) {
        if (ledState && ledTimeoutState)  {
            anim_tick tick = getCurrentProfile()->tick;
            if (tick) tick(ledColors);
        }

        executeKeypress();

        ledsNeedUpdate = true;
    }
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

