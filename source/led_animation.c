#include "ch.h"
#include "led_animation.h"
#include "led_state.h"


#define FPS_TO_TIEOUT(X) (ANIMATION_TIMER_FREQUENCY/X)

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

    uint8_t profileFps = getCurrentProfile()->fps;
    if (profileFps == REACTIVE_FPS) {
        profileFps = getReactiveFps();
    }
    
    if (tickCount % FPS_TO_TIEOUT(profileFps) == 0)
        executeProfile();

    if (tickCount % FPS_TO_TIEOUT(getOneShotEffect()->fps) == 0)
        executeOneShotEffect();

    tickCount++;
}


void led_anim_init() {
    led_state_init();

    executeInit();
    executeProfile();

    gptStart(&GPTD_BFTM1, &lightAnimationConfig);
    gptStartContinuous(&GPTD_BFTM1, 1);
}
