#pragma once

#include "light_utils.h"
#include "profiles.h"

void led_anim_init(void);

typedef enum { POWER_BATT, POWER_USB, POWER_MAX } PowerPlan;

void mainInitDoneCallback(void);
void led_state_init(void);
void switchProfile(int profile);
void disableLeds(void);
void enableLeds(void);
void toggleLeds(void);
void nextProfile(void);
void prevProfile(void);
void bltConnected(void);
void bltConnecting(uint8_t state);
void brightnessDown(void);
void brightnessUp(void);
void setBrightness(uint8_t bn);
void setGamingMode(bool mode);
void setCapsState(bool state);
int16_t getBrightness(void);
void keyPressedCallback(uint8_t keyPos);
void setPowerPlan(PowerPlan pp);
void setLocked(bool locked);
Profile* getCurrentProfile(void);
uint8_t getProfileCount(void);
uint8_t getCurrentProfileIndex(void);
void ledPostProcess(void);
led_t* getLedsToDisplay(void);
void displayTemp(void);
void displayTime(void);

void executeInit(void);
void executeKeypress(void);

void updateTimeout(void);

void displayNumber(int value);

void registerOverlapEffect(Effect* effect);
bool overlapEffectIsActive(void);
Effect* getOverlapEffect(void);

