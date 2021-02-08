/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "board.h"
#include "hal.h"
#include "ch.h"
#include "string.h"
#include "ap2_qmk_led.h"
#include "light_utils.h"
#include "profiles.h"
#include "extra_profiles.h"
#include "miniFastLED.h"


static void columnCallback(void/*GPTDriver* _driver*/);
static void animationCallback(GPTDriver* driver);
static void executeMsg(msg_t msg);
static void switchProfile(int profile);
static void executeProfile(void);
static void executeKeypress(void);
static void keyPressedCallback(void);
static void disableLeds(void);
static void enableLeds(void);
static void toggleLeds(void);
static void setLocked(void);
static void setProfile(void);
static void setBrightness(void);
static void setPowerPlan(void);
static void setWeather(void);
static void bltConnecting(void);
static void ledPostProcess(void);
static void nextProfile(void);
static void prevProfile(void);

static void goIntoIAP(void);

static bool ledState = false;
static bool ledTimeoutState = true;
static bool capsState = false;
// 0 means that it is not connecting
static uint8_t bltState = 0;
static systime_t bltConnSpeed = 500;
static systime_t bltBroadSpeed = 250;
static bool bltLedOn = false;
static systime_t bltLedLastSwitched = 0;

static int brightness = 100;
static bool gamingMode = false;

#define LED_TIMEOUT_BATTERY 180
#define LED_TIMEOUT_USB 1200
static systime_t lastKeypress;

typedef enum { POWER_USB, POWER_BATTERY } PowerPlan;
PowerPlan powerPlan = POWER_BATTERY;


ioline_t ledColumns[NUM_COLUMN] = {
  LINE_LED_COL_1, 
  LINE_LED_COL_2, 
  LINE_LED_COL_3, 
  LINE_LED_COL_4, 
  LINE_LED_COL_5, 
  LINE_LED_COL_6, 
  LINE_LED_COL_7, 
  LINE_LED_COL_8, 
  LINE_LED_COL_9, 
  LINE_LED_COL_10,
  LINE_LED_COL_11,
  LINE_LED_COL_12,
  LINE_LED_COL_13,
  LINE_LED_COL_14
};

ioline_t ledRows[NUM_ROW * 4] = {
  LINE_LED_ROW_1_R,
  LINE_LED_ROW_1_G,
  LINE_LED_ROW_1_B,
  0,
  LINE_LED_ROW_2_R,
  LINE_LED_ROW_2_G,
  LINE_LED_ROW_2_B,
  0,
  LINE_LED_ROW_3_R,
  LINE_LED_ROW_3_G,
  LINE_LED_ROW_3_B,
  0,
  LINE_LED_ROW_4_R,
  LINE_LED_ROW_4_G,
  LINE_LED_ROW_4_B,
  0,
  LINE_LED_ROW_5_R,
  LINE_LED_ROW_5_G,
  LINE_LED_ROW_5_B,
  0
};

#define ANIMATION_TIMER_FREQUENCY   60



typedef void (*anim_tick)( led_t* );
typedef void (*anim_keypress)( uint8_t col, uint8_t row, led_t* keyColors );
typedef void (*anim_init)( led_t* );

typedef struct {
  uint8_t fps;
  anim_tick tick;
  anim_init init;
  anim_keypress keypress;
} Profile;

static Profile* getCurrentProfile(void);

static Profile profiles[] = {
  { 30, anim_rain, rain_init, 0 },
  { 30, anim_storm, storm_init, 0 },
  { 30, animatedRainbowFlow, 0, 0 },
  { 30, anim_breathing, breathing_init, pressed_breathing },
  { 30, anim_snowing, snowing_init, 0 },
  { 6,  anim_stars, 0, 0 },
  { 30, anim_sunny, sunny_init, 0 },
  { 0,  anim_liveWeather, 0, 0 },
};

static Profile lockedProfile = { 30, anim_locked, locked_init, 0 };


static uint8_t currentProfile = 0;
static uint8_t amountOfProfiles = sizeof(profiles)/sizeof(Profile);

static led_t ledColors[70];
static led_t ledColorsPost[70];
static led_t ledFinal[70];


typedef struct {
  uint8_t col;
  uint8_t row;
} keypress;

#define pressedKeysBufSize 35
static keypress pressedKeys[pressedKeysBufSize];
static uint8_t pressedKeyCnt = 0;

static bool isLocked = false;

// Lighting animation refresh timer
static const GPTConfig lightAnimationConfig = {
  .frequency = ANIMATION_TIMER_FREQUENCY,
  .callback = animationCallback
};

static const SerialConfig usart1Config = {
  .speed = 115200
};

static uint8_t commandBuffer[64];


static void goIntoIAP() {
    *((uint32_t*)0x20001ffc) = 0x0000fab2;
    __disable_irq();
    NVIC_SystemReset();
}


/*
 * Execute action based on a message
 */
void executeMsg(msg_t msg){
  switch (msg) {
    case LED_TOGGLE:
      toggleLeds();
      break;

    case LED_NEXT_PROFILE:
      nextProfile();
      break;

    case LED_PREV_PROFILE:
      prevProfile();
      break;

    case LED_SET_PROFILE:
      setProfile();
      break;

    case LED_GET_PROFILE:
      sdWrite(&SD1, &currentProfile, 1);
      break;

    case LED_GET_PROFILE_COUNT:
      sdWrite(&SD1, &amountOfProfiles, 1);
      break;

    case LED_KEY_PRESSED:
      keyPressedCallback();
      break;

    case LED_CAPS_ON:
      capsState = true;
      break;

    case LED_CAPS_OFF:
      capsState = false;
      break;

    case LED_BLT_CONNECTING:
      bltConnecting();
      break;

    case LED_BLT_CONNECTED:
      bltState = 0;
      break;

    case LED_BRIGHT_DOWN:
      brightness -= 20;
      if (brightness < 10)
        brightness = 10;
      break;

    case LED_BRIGHT_UP:
      brightness += 20;
      if (brightness > 100)
        brightness = 100;
      break;

    case LED_SET_BRIGHT:
      setBrightness();
      break;

    case LED_GET_BRIGHT:
      sdPut(&SD1, (uint8_t)(brightness));
      break;

    case LED_GAMING_ON:
      gamingMode = true;
      break;

    case LED_GAMING_OFF:
      gamingMode = false;
      break;

    case LED_SET_LOCKED:
      setLocked();
      break;

    case LED_IAP_MODE:
      goIntoIAP();
      break;

    case LED_POWER_PLAN:
      setPowerPlan();
      break;

    case LED_UPDATE_WEATHER:
      setWeather();
      break;

    default:
      break;
  }
}


Profile* getCurrentProfile() {
  if (isLocked) {
    return &lockedProfile;
  }
  else {
    return &profiles[currentProfile];
  }
}


void setProfile(){
  size_t bytesRead;
  bytesRead = sdReadTimeout(&SD1, commandBuffer, 1, 10000);

  if(bytesRead == 1) {
    if(commandBuffer[0] < amountOfProfiles) {
      switchProfile(commandBuffer[0]);
    }
  }
}

void bltConnecting() {
  size_t bytesRead;
  bytesRead = sdReadTimeout(&SD1, commandBuffer, 1, 10000);

  if(bytesRead == 1){
    bltState = commandBuffer[0];

    bltLedOn = true;
    bltLedLastSwitched = sysTimeMs();
  }
}


static void setBrightness() {
  size_t bytesRead;
  bytesRead = sdReadTimeout(&SD1, commandBuffer, 1, 10000);

  if(bytesRead == 1) {
    brightness = commandBuffer[0];
  }
}


static void setLocked() {
  size_t bytesRead;
  bytesRead = sdReadTimeout(&SD1, commandBuffer, 1, 10000);

  if(bytesRead == 1) {
    isLocked = commandBuffer[0];
    executeInit();
  }
}

static void setPowerPlan() {
  size_t bytesRead;
  bytesRead = sdReadTimeout(&SD1, commandBuffer, 1, 10000);

  if(bytesRead == 1) {
    powerPlan = commandBuffer[0];
  }
}


static void setWeather(void) {
  size_t bytesRead;
  bytesRead = sdReadTimeout(&SD1, commandBuffer, sizeof(commandBuffer), 5000);

  if(bytesRead == sizeof(WeatherData)) {
    setWeatherData((WeatherData*) commandBuffer);
  }
}


void keyPressedCallback() {
  lastKeypress = sysTimeMs();

  if (ledTimeoutState == false) {
    executeInit();
  }

  ledTimeoutState = true;

  size_t bytesRead;
  bytesRead = sdReadTimeout(&SD1, commandBuffer, 1, 10000);

  if(bytesRead == 1) {
    uint8_t keyPos = commandBuffer[0];

    if (pressedKeyCnt < pressedKeysBufSize) {
      pressedKeys[pressedKeyCnt].row = keyPos & 0xF;
      pressedKeys[pressedKeyCnt].col = (keyPos >> 4) & 0xF; 
      pressedKeyCnt++;
    }
  }

}


void switchProfile(int profile) {
  currentProfile = profile % amountOfProfiles;

  executeInit();
}

void nextProfile() {
  switchProfile(currentProfile + 1);
}

void prevProfile() {
  switchProfile(currentProfile - 1);
}


/*
 * Execute current profile
 */
void executeProfile(){
  if (ledState && ledTimeoutState) {
    anim_tick tick = getCurrentProfile()->tick;

    if (tick)
      tick(ledColors);
  }

  executeKeypress();

  ledPostProcess();
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
      systime_t bltLedRate = bltState <= 4 ? bltConnSpeed : bltBroadSpeed;

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


void executeInit() {
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

/*
 * Turn off all leds
 */
void disableLeds() {
  ledState = false;
}

/*
 * Turn on all leds
 */
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


void animationCallback(GPTDriver* _driver) {
  (void)_driver;

  if (ledTimeoutState) {
    int ledTimeout = powerPlan == POWER_BATTERY ?
      LED_TIMEOUT_BATTERY : LED_TIMEOUT_USB;

    if (sysTimeMs() - lastKeypress >= ledTimeout * 1000) {
      ledTimeoutState = false;
      memset(ledColors, 0, NUM_COLUMN * NUM_ROW * sizeof(led_t));
    }
  }

  uint8_t fps = getCurrentProfile()->fps;
  if (fps == 0) {
    fps = getReactiveFps();
  }
  
  gptChangeInterval(_driver, ANIMATION_TIMER_FREQUENCY/fps);
  
  executeProfile();
}


inline uint8_t sPWM(uint8_t cycle, uint8_t currentCount, uint8_t start, ioline_t port){
  if (start+cycle>0xFF) start = 0xFF - cycle;
  if (start <= currentCount && currentCount < start+cycle) {
    palSetLine(port);
    return 1;
  }
  else {
    palClearLine(port);
    return 0;
  }
}


/*
 * Thread 1.
 */
THD_WORKING_AREA(waThread1, 128);
__attribute__((noreturn)) THD_FUNCTION(Thread1, arg) {
  (void)arg;

  while(true) {
    columnCallback();
  }
}


/*
#define REFRESH_FREQUENCY 180

static const GPTConfig bftm0Config = {	
  .frequency = NUM_COLUMN * REFRESH_FREQUENCY * 2 * 16,	
  .callback = columnCallback	
};
*/


void columnCallback() {
  static uint8_t columnPWMCount = 0;
  static uint8_t currentColumn = 0;

  palClearLine(ledColumns[currentColumn]);

  uint8_t colHasBeenSet = 0;
  
  currentColumn = (currentColumn+1) % NUM_COLUMN;
  if (columnPWMCount < 255) {

    for (size_t row = 0; row < NUM_ROW; row++) {
      const size_t ledIndex = currentColumn + (NUM_COLUMN * row);
      const led_t keyLED = ledFinal[ledIndex];

      colHasBeenSet += sPWM(keyLED.red, columnPWMCount, 0, ledRows[row << 2]);
      colHasBeenSet += sPWM(keyLED.green, columnPWMCount, keyLED.red, ledRows[(row << 2) | 1]);
      colHasBeenSet += sPWM(keyLED.blue, columnPWMCount, keyLED.red + keyLED.green, ledRows[(row << 2) | 2]);
    }

    columnPWMCount++;
  }
  else {
    columnPWMCount = 0;
  }

  if (colHasBeenSet)
    palSetLine(ledColumns[currentColumn]);
}



/*
 * Application entry point.
 */
int main(void) {
  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */

  halInit();
  chSysInit();

  lastKeypress = sysTimeMs();

  memset(ledColors, 0, NUM_COLUMN * NUM_ROW * sizeof(led_t));

  executeInit();
  executeProfile();


  palClearLine(LINE_LED_PWR);
  sdStart(&SD1, &usart1Config);

  // Setup Animation Timer
  gptStart(&GPTD_BFTM1, &lightAnimationConfig);
  gptStartContinuous(&GPTD_BFTM1, 1);

  // Setup Column Multiplex Timer	
  // gptStart(&GPTD_BFTM0, &bftm0Config);	
  // gptStartContinuous(&GPTD_BFTM0, 1);

  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);
  /* This is now the idle thread loop, you may perform here a low priority
     task but you must never try to sleep or wait in this loop. Note that
     this tasks runs at the lowest priority level so any instruction added
     here will be executed after all other tasks have been started.*/

  palSetLine(LINE_LED_PWR);

  while (true) {
    //columnCallback();

    //while(!sdGetWouldBlock(&SD1)){	
      msg_t msg;	
      msg = sdGet(&SD1);	
      if(msg >= MSG_OK){	
        executeMsg(msg);	
      }	
    //}
  }
}
