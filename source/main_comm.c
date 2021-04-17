#include "common_utils.h"
#include "led_state.h"
#include "main_comm.h"
#include "profiles.h"


static void readLocked(void);
static void readProfile(void);
static void readBrightness(void);
static void readPowerPlan(void);
static void readWeather(void);
static void readKeypress(void);
static void readBltConnecting(void);
static void goIntoIAP(void);


enum LedMsgCode {           // Messages:
    LED_TOGGLE = 1,         // 1 byte: 0 - off, 1 - on
    LED_NEXT_PROFILE,       // 0 byte
    LED_PREV_PROFILE,       // 0 byte
    LED_SET_PROFILE,        // 1 byte: profile
    LED_GET_PROFILE,        // 0 byte;  response - 1 byte: message
    LED_GET_PROFILE_COUNT,  // 0 byte;  response - 1 byte: message
    LED_KEY_PRESSED,        // 1 byte: col (4 bits) + row (4 bits)
    LED_CAPS_ON,            // 0 byte
    LED_CAPS_OFF,           // 0 byte
    LED_BLT_CONNECTING,     // 1 byte: 1-4
    LED_BLT_CONNECTED,      // 0 byte
    LED_BRIGHT_DOWN,        // 0 byte
    LED_BRIGHT_UP,          // 0 byte
    LED_SET_BRIGHT,         // 1 byte: brightness (0-100)
    LED_GET_BRIGHT,
    LED_GAMING_ON,
    LED_GAMING_OFF,
    LED_SET_LOCKED,
    LED_IAP_MODE,
    LED_POWER_PLAN,         // 1 byte; 0 - normal, 1 - power saving
    LED_UPDATE_WEATHER,
    LED_SHOW_TEMP,
    LED_SHOW_TIME
};


/*
 * Execute action based on a message
 */
void main_comm_executeMsg(msg_t msg){
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
            readProfile();
            break;

        case LED_GET_PROFILE: {
            uint8_t currentProfile = getCurrentProfileIndex();
            sdWrite(&SD1, &currentProfile, 1);
        }
            break;
            
        case LED_GET_PROFILE_COUNT: {
            uint8_t profileCount = getProfileCount();
            sdWrite(&SD1, &profileCount, 1);
        }
            break;

        case LED_KEY_PRESSED:
            readKeypress();
            break;

        case LED_CAPS_ON:
            setCapsState(true);
            break;

        case LED_CAPS_OFF:
            setCapsState(false);
            break;

        case LED_BLT_CONNECTING:
            readBltConnecting();
            break;

        case LED_BLT_CONNECTED:
            bltConnected();
            break;

        case LED_BRIGHT_DOWN:
            brightnessDown();
            break;

        case LED_BRIGHT_UP:
            brightnessUp();
            break;

        case LED_SET_BRIGHT:
            readBrightness();
            break;

        case LED_GET_BRIGHT:
            sdPut(&SD1, (uint8_t)(getBrightness()));
            break;

        case LED_GAMING_ON:
            setGamingMode(true);
            break;

        case LED_GAMING_OFF:
            setGamingMode(false);
            break;

        case LED_SET_LOCKED:
            readLocked();
            break;

        case LED_IAP_MODE:
            goIntoIAP();
            break;

        case LED_POWER_PLAN:
            readPowerPlan();
            break;

        case LED_UPDATE_WEATHER:
            readWeather();
            break;

        case LED_SHOW_TEMP:
            displayTemp();
            break;

        case LED_SHOW_TIME:
            displayTime();
            break;

        default:
            break;
    }
}

static uint8_t commandBuffer[64];


static void goIntoIAP() {
    *((uint32_t*)0x20001ffc) = 0x0000fab2;
    __disable_irq();
    NVIC_SystemReset();
}


static void readProfile(){
    size_t bytesRead;
    bytesRead = sdReadTimeout(&SD1, commandBuffer, 1, 10000);

    if(bytesRead == 1) {
        switchProfile(commandBuffer[0]);
    }
}


static void readBltConnecting() {
    size_t bytesRead;
    bytesRead = sdReadTimeout(&SD1, commandBuffer, 1, 10000);

    if(bytesRead == 1){
        bltConnecting(commandBuffer[0]);
    }
}


static void readKeypress() {
    size_t bytesRead;
    bytesRead = sdReadTimeout(&SD1, commandBuffer, 1, 10000);

    if(bytesRead == 1) {
        uint8_t keyPos = commandBuffer[0];
        keyPressedCallback(keyPos);
    }
}


static void readBrightness() {
    size_t bytesRead;
    bytesRead = sdReadTimeout(&SD1, commandBuffer, 1, 10000);

    if(bytesRead == 1) {
        setBrightness(commandBuffer[0]);
    }
}

static void readPowerPlan() {
    size_t bytesRead;
    bytesRead = sdReadTimeout(&SD1, commandBuffer, 1, 10000);

    if(bytesRead == 1) {
        setPowerPlan((PowerPlan)commandBuffer[0]);
    }
}


static void readLocked() {
    size_t bytesRead;
    bytesRead = sdReadTimeout(&SD1, commandBuffer, 1, 10000);

    if(bytesRead == 1) {
        setLocked(commandBuffer[0]);
        executeInit();
    }
}


static void readWeather(void) {
    sdReadTimeout(&SD1, commandBuffer, sizeof(commandBuffer), 5000);

    setWeatherData((WeatherData*) commandBuffer);

    Profile* currProfile = getCurrentProfile();
    if (currProfile->tick == prof_liveWeather_tick) {
        executeInit();
    }
}


