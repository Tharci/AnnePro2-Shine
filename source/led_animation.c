#include "ch.h"
#include "led_animation.h"
#include "led_state.h"


#define ANIMATION_TIMER_FREQUENCY   60


static void animationCallback(GPTDriver* driver);


// Lighting animation refresh timer
static const GPTConfig lightAnimationConfig = {
    .frequency = ANIMATION_TIMER_FREQUENCY,
    .callback = animationCallback
};


static void animationCallback(GPTDriver* _driver) {
    (void)_driver;

    updateTimeout();

    uint8_t fps = getCurrentProfile()->fps;
    if (fps == REACTIVE_FPS) {
        fps = getReactiveFps();
    }
    
    gptChangeInterval(_driver, ANIMATION_TIMER_FREQUENCY/fps);
    executeProfile();
}


void led_anim_init() {
    led_state_init();

    executeInit();
    executeProfile();

    gptStart(&GPTD_BFTM1, &lightAnimationConfig);
    gptStartContinuous(&GPTD_BFTM1, 1);
}
