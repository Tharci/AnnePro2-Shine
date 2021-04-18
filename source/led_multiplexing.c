#include "led_multiplexing.h"
#include "board.h"
#include "hal.h"
#include "ch.h"
#include "light_utils.h"
#include "led_state.h"


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


/*
#define REFRESH_FREQUENCY 180

static const GPTConfig bftm0Config = {	
    .frequency = NUM_COLUMN * REFRESH_FREQUENCY * 2 * 16,	
    .callback = columnCallback	
};
*/

static void columnCallback(void);


THD_WORKING_AREA(waThread1, 128);
    __attribute__((noreturn)) THD_FUNCTION(Thread1, arg) {
    (void)arg;

    while(true) {
        columnCallback();
    }
}


void led_multiplexing_init() {
    // Setup Column Multiplex Timer	
    // gptStart(&GPTD_BFTM0, &bftm0Config);	
    // gptStartContinuous(&GPTD_BFTM0, 1);

    chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);
}

inline bool sPWM(uint8_t value, uint8_t pwmCounter, uint8_t column, bool cycleState, ioline_t port) {
    if (pwmCounter < value && (column + cycleState) % 2) {
        palSetLine(port);
        return true;
    }
    else {
        palClearLine(port);
        return false;
    }
}


static void columnCallback() {
    static uint8_t pwmCounter = 0;
    static uint8_t currentColumn = 0;
    static bool colHasBeenSet = true;

    /* 
     * @var cycleState Keeps track of which columns are updated in the current cycle (odd or even).
     *                 Only odd or only even columns are updated in one cycle. The calculations are 
     *                 done on the remaining columns aswell just to give a little time for the voltage to drop.
     *                 This method prevents bleeding.
     */
    static bool cycleState = false;

    if (colHasBeenSet)
        palClearLine(ledColumns[currentColumn]);


    currentColumn = (currentColumn + 1) % NUM_COLUMN;
    if (currentColumn == 0) {
        cycleState = !cycleState;
    }

    colHasBeenSet = false;
    
    for (uint8_t row = 0; row < NUM_ROW; row++) {
        const led_t keyLED = getLedsToDisplay()[currentColumn + NUM_COLUMN * row];

        colHasBeenSet += sPWM(keyLED.red, pwmCounter, currentColumn, cycleState, ledRows[row << 2]);
        colHasBeenSet += sPWM(keyLED.green, pwmCounter, currentColumn, cycleState, ledRows[(row << 2) | 1]);
        colHasBeenSet += sPWM(keyLED.blue, pwmCounter, currentColumn, cycleState, ledRows[(row << 2) | 2]);
    }

    if ((currentColumn + cycleState) % 2) 
        pwmCounter++;

    if (colHasBeenSet && (currentColumn + cycleState) % 2)
        palSetLine(ledColumns[currentColumn]);
}