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


static void columnCallback(GPTDriver* driver);
static void animationCallback(GPTDriver* driver);
static void executeMsg(msg_t msg);
static void switchProfile(uint8_t profile);
static void executeProfile(void);
static void executeKeypress(void);
static void keyPressedCallback(void);
static void disableLeds(void);
static void enableLeds(void);
static void toggleLeds(void);
static void setLocked(void);
static void setProfile(void);
static void setBrightness(void);
static void bltConnecting(void);
static void ledPostProcess(void);
static void nextProfile(void);
static void prevProfile(void);

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

#define LED_TIMEOUT 180
static systime_t lastKeypress;


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

#define REFRESH_FREQUENCY           150
#define ANIMATION_TIMER_FREQUENCY   60



typedef void (*anim_tick)( led_t* );
typedef void (*anim_keypress)( uint8_t col, uint8_t row, led_t* keyColors );


typedef struct {
  anim_tick tick;
  anim_keypress keypress;
  uint8_t fps;
} profile;

static profile getCurrentProfile(void);

profile profiles[] = {
  { anim_rain, 0, 30 },
  { anim_thunder, 0, 30 },
  { animatedRainbowFlow, 0, 30 },
  { anim_breathing, pressed_breathing, 30 },
  { anim_snowing, 0, 30 }
};

static profile lockedProfile = { anim_locked, 0, 60 };


static uint8_t currentProfile = 0;
static uint8_t amountOfProfiles = sizeof(profiles)/sizeof(profile);

static led_t ledColors[70];
static uint32_t currentColumn = 0;
static uint32_t columnPWMCount = 0;


typedef struct {
  uint8_t col;
  uint8_t row;
} keypress;

#define pressedKeysBufSize 35
static keypress pressedKeys[pressedKeysBufSize];
static uint8_t pressedKeyCnt = 0;

static bool isLocked = false;

// BFTM0 Configuration, this runs at 15 * REFRESH_FREQUENCY Hz
static const GPTConfig bftm0Config = {
  .frequency = NUM_COLUMN * REFRESH_FREQUENCY * 2 * 16,
  .callback = columnCallback
};

// Lighting animation refresh timer
static const GPTConfig lightAnimationConfig = {
  .frequency = ANIMATION_TIMER_FREQUENCY,
  .callback = animationCallback
};

static const SerialConfig usart1Config = {
  .speed = 115200
};

static uint8_t commandBuffer[64];

/*
 * Thread 1.
 */
 
THD_WORKING_AREA(waThread1, 128);
__attribute__((noreturn)) THD_FUNCTION(Thread1, arg) {
  (void)arg;

  while(true){
    msg_t msg;
    msg = sdGet(&SD1);
    if(msg >= MSG_OK){
      executeMsg(msg);
    }
  }
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

    default:
      break;
  }
}


profile getCurrentProfile() {
  if (isLocked) {
    return lockedProfile;
  }
  else {
    return profiles[currentProfile];
  }
}


/*
 * Set profile and execute it
 */
void setProfile(){
  size_t bytesRead;
  bytesRead = sdReadTimeout(&SD1, commandBuffer, 1, 10000);

  if(bytesRead == 1) {
    if(commandBuffer[0] < amountOfProfiles) {
      switchProfile(commandBuffer[0]);
    }
  }

  // set colors without turning on leds (if we have a saved profile off by default in eeprom)
  executeProfile();
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
  }
}

void keyPressedCallback() {
  lastKeypress = sysTimeMs();
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


void switchProfile(uint8_t profile){
  currentProfile = profile % amountOfProfiles;
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
  chSysLock();

  if (!isLocked) {
    if (ledState && ledTimeoutState) {
      getCurrentProfile().tick(ledColors);
    }

    executeKeypress();
    ledPostProcess();
  } else {
    if (ledState && ledTimeoutState) {
      lockedProfile.tick(ledColors);
    }

    executeKeypress();
  }

  chSysUnlock();
}


void ledPostProcess() {
  if(!(ledState & ledTimeoutState))
    memset(ledColors, 0, NUM_COLUMN * NUM_ROW * sizeof(led_t));


  for (int i = 0; i < NUM_COLUMN * NUM_ROW; i++) {
    ledColors[i].red = (ledColors[i].red * brightness) / 100;
    ledColors[i].green = (ledColors[i].green * brightness) / 100;
    ledColors[i].blue = (ledColors[i].blue * brightness) / 100;
  }


  static const led_t capsColor = { 140, 7, 7 };
  if (capsState) {
    ledColors[28] = capsColor;
  }


  static const led_t bltColor = {0, 255, 30};

  if (bltState) {
    systime_t bltLedRate = bltState <= 4 ? bltConnSpeed : bltBroadSpeed;

    if (sysTimeMs() - bltLedLastSwitched > bltLedRate) {
      bltLedOn = !bltLedOn;
      bltLedLastSwitched = sysTimeMs();
    }

    if (bltLedOn)
      ledColors[(bltState - 1) % 4 + 1] = bltColor;
  }


  static const uint8_t gamingArrows[] = { 54, 66, 67, 68 };
  static const led_t gamingArrowLedColor = { 110, 5, 5 };
  if (gamingMode) {
    for (uint8_t i = 0; i < LEN(gamingArrows); i++) {
      ledColors[gamingArrows[i]] = gamingArrowLedColor;
    }
  }
}


void executeKeypress() {
  anim_keypress callback = getCurrentProfile().keypress;
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


void handleMsgCallback(GPTDriver* _driver) {
  
}


/*
 * Update lighting table as per animation
 */
void animationCallback(GPTDriver* _driver) {
  chSysLock();
/*
  while(!sdGetWouldBlock(&SD1)){
    msg_t msg;
    msg = sdGet(&SD1);
    if(msg >= MSG_OK){
      executeMsg(msg);
    }
  }
*/
  chSysUnlock();

  if (ledTimeoutState) {
    systime_t currTime = sysTimeMs();
    if (currTime - lastKeypress >= LED_TIMEOUT * 1000) {
      ledTimeoutState = false;
    }
  }
  
  gptChangeInterval(_driver, ANIMATION_TIMER_FREQUENCY/getCurrentProfile().fps);
  
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


void columnCallback(GPTDriver* _driver) {
  (void)_driver;
  palClearLine(ledColumns[currentColumn]);

  uint8_t rowHasBeenSet = 0;
  
  currentColumn = (currentColumn+1) % NUM_COLUMN;
  if (columnPWMCount < 255) {
    for (size_t row = 0; row < NUM_ROW; row++) {
      const size_t ledIndex = currentColumn + (NUM_COLUMN * row);
      const led_t keyLED = ledColors[ledIndex];

      rowHasBeenSet += sPWM(keyLED.red, columnPWMCount, 0, ledRows[row << 2]);
      rowHasBeenSet += sPWM(keyLED.green, columnPWMCount, keyLED.red, ledRows[(row << 2) | 1]);
      rowHasBeenSet += sPWM(keyLED.blue, columnPWMCount, keyLED.red + keyLED.green, ledRows[(row << 2) | 2]);
    }

    columnPWMCount++;
  }
  else {
    columnPWMCount = 0;
  }

  if (rowHasBeenSet)
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

  for (int i = 0; i < NUM_ROW * NUM_COLUMN; i++) {
    ledColors[i].red   = 0;
    ledColors[i].green = 0;
    ledColors[i].blue  = 0;
  }

  executeProfile();


  palClearLine(LINE_LED_PWR);
  sdStart(&SD1, &usart1Config);

  // Setup Column Multiplex Timer
  //gptStart(&GPTD_BFTM0, &bftm0Config);
  //gptStartContinuous(&GPTD_BFTM0, 1);

  // Setup Animation Timer
  gptStart(&GPTD_BFTM1, &lightAnimationConfig);
  gptStartContinuous(&GPTD_BFTM1, 1);

  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO + 10, Thread1, NULL);
  /* This is now the idle thread loop, you may perform here a low priority
     task but you must never try to sleep or wait in this loop. Note that
     this tasks runs at the lowest priority level so any instruction added
     here will be executed after all other tasks have been started.*/

  palSetLine(LINE_LED_PWR);
  disableLeds();

  while (true) {
    columnCallback(0);
  }
}
