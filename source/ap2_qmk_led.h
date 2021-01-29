#pragma once

#include "light_utils.h"

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
    LED_SET_LOCKED
};

#define LEN(a) (sizeof(a)/sizeof(*a))

typedef struct {
    int8_t x, y;
} pos_i;


systime_t sysTimeMs(void);
systime_t sysTimeS(void);


unsigned long randInt(void);


