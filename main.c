/******************************************************************************
 *
 * File:
 *    main.c
 *
 * Authors:
 *    Konrad Jaworski
 *    Przemysław Rudowicz
 *    Jakub Plich
 *
 *
 * Description:
 *    Snake game on LPC1768/9
 *
 *****************************************************************************/

/******************************************************************************
 * Includes
 *****************************************************************************/

#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_timer.h"

#include "joystick.h"
#include "pca9532.h"
#include "acc.h"
#include "rotary.h"
#include "led7seg.h"
#include "oled.h"
#include "rgb.h"
#include "light.h"
#include "rotary.h"

#include <stdlib.h>


/******************************************************************************
 * Typedefs and defines
 *****************************************************************************/

#define NOTE_PIN_HIGH() GPIO_SetValue(0, 1<<26);
#define NOTE_PIN_LOW()  GPIO_ClearValue(0, 1<<26);


/*****************************************************************************
 * Global variables
 ****************************************************************************/

Bool OLED_COLOR = FALSE;
Bool isJoystickBroken = FALSE;
Bool isAccBroken = FALSE;

uint32_t seedForSrand = 0;

static uint32_t notes[] = {
        2272, // A - 440 Hz
        2024, // B - 494 Hz
        3816, // C - 262 Hz
        3401, // D - 294 Hz
        3030, // E - 330 Hz
        2865, // F - 349 Hz
        2551, // G - 392 Hz
        1136, // a - 880 Hz
        1012, // b - 988 Hz
        1912, // c - 523 Hz
        1703, // d - 587 Hz
        1517, // e - 659 Hz
        1432, // f - 698 Hz
        1275, // g - 784 Hz
};

static uint8_t *song_up = (uint8_t *) "C4,";
static uint8_t *song_down = (uint8_t *) "D4,";


/*****************************************************************************
 * Structures definition
 ****************************************************************************/

struct Point {
    uint8_t x;
    uint8_t y;
};

struct Fruit {
    uint8_t x1;
    uint8_t y1;

    uint8_t x2;
    uint8_t y2;

    uint8_t x3;
    uint8_t y3;

    uint8_t x4;
    uint8_t y4;
};


struct Snake {
    struct Point *snakeBody;
    struct Fruit fruit;
    uint8_t size;
    uint8_t lastDirection;
    Bool isBlocked;
    uint16_t score;
};


static void playNote(uint32_t note, uint32_t durationMs) {

    uint32_t t = 0;

    if (note > 0) {

        while (t < (durationMs * 1000)) {
            NOTE_PIN_HIGH();
            Timer0_us_Wait(note / 2);
            //delay32Us(0, note / 2);

            NOTE_PIN_LOW();
            Timer0_us_Wait(note / 2);
            //delay32Us(0, note / 2);

            t += note;
        }

    } else {
        Timer0_Wait(durationMs);
        //delay32Ms(0, durationMs);
    }
}

static uint32_t getNote(uint8_t ch) {
    if (ch >= 'A' && ch <= 'G')
        return notes[ch - 'A'];

    if (ch >= 'a' && ch <= 'g')
        return notes[ch - 'a' + 7];

    return 0;
}

static uint32_t getDuration(uint8_t ch) {
    if (ch < '0' || ch > '9')
        return 400;
    /* number of ms */
    return (ch - '0') * 200;
}

static uint32_t getPause(uint8_t ch) {
    switch (ch) {
        case '+':
            return 0;
        case ',':
            return 5;
        case '.':
            return 20;
        case '_':
            return 30;
        default:
            return 5;
    }
}

static void playSong(uint8_t *song) {
    uint32_t note = 0;
    uint32_t dur = 0;
    uint32_t pause = 0;
    /*
     * A song is a collection of tones where each tone is
     * a note, duration and pause, e.g.
     *
     * "E2,F4,"
     */
    while (*song != '\0') {
        note = getNote(*song++);
        if (*song == '\0')
            break;
        dur = getDuration(*song++);
        if (*song == '\0')
            break;
        pause = getPause(*song++);

        playNote(note, dur);
        Timer0_Wait(pause);
    }
}

static void showScore(uint16_t score) {
    pca9532_setLeds(score, 0xffff);
}


struct Point createPoint(int x, int y) {
    struct Point point;
    point.x = x;
    point.y = y;
    return point;
}

void clearScreenWithFrame() {
    oled_clearScreen(OLED_COLOR);
    for (int i = 0; i < OLED_DISPLAY_HEIGHT - 1; i++) {
        oled_putPixel(0, i, !OLED_COLOR);
        oled_putPixel(OLED_DISPLAY_WIDTH - 1, i, !OLED_COLOR);
    }

    for (int i = 0; i < OLED_DISPLAY_WIDTH - 1; i++) {
        oled_putPixel(i, 0, !OLED_COLOR);
        oled_putPixel(i, OLED_DISPLAY_HEIGHT - 1, !OLED_COLOR);
    }
}


struct Snake *createSnake(uint8_t size) {
    struct Point *snakeBody;
    snakeBody = malloc(size * sizeof(struct Point));
    for (int i = 0; i < size; i++) {
        struct Point p = createPoint(48 + i, 50);
        snakeBody[i] = p;
    }
    struct Snake *newSnake;
    newSnake = malloc(sizeof(struct Snake));
    newSnake->size = size;
    newSnake->snakeBody = snakeBody;
    newSnake->lastDirection = JOYSTICK_UP;
    struct Fruit f;
    f.x1 = 10;
    f.y1 = 10;

    f.x2 = f.x1 + 1;
    f.y2 = f.y1;

    f.x3 = f.x1;
    f.y3 = f.y1 + 1;

    f.x4 = f.x1 + 1;
    f.y4 = f.y1 + 1;

    newSnake->fruit = f;

    newSnake->isBlocked = TRUE;

    newSnake->score = 0;

    return newSnake;
}

void deleteOldSnake(struct Snake *s) {
    int size = 30;
    s->size = size;
    struct Point *snakeBody;
    snakeBody = malloc(size * sizeof(struct Point));
    for (int i = 0; i < size; i++) {
        struct Point p = createPoint(48 + i, 32);
        snakeBody[i] = p;
    }

    s->snakeBody = snakeBody;
    clearScreenWithFrame();
}

struct Fruit createFruit(int x, int y) {
    struct Fruit f;
    f.x1 = x;
    f.y1 = y;

    f.x2 = f.x1 + 1;
    f.y2 = f.y1;

    f.x3 = f.x1;
    f.y3 = f.y1 + 1;

    f.x4 = f.x1 + 1;
    f.y4 = f.y1 + 1;
    return f;
}

void incSnake(struct Snake *s) {
    s->size += 1;
    struct Point *snakeBody2;
    snakeBody2 = malloc(s->size * sizeof(struct Point));
    for (int i = 0; i < s->size - 1; i++) {
        snakeBody2[i] = s->snakeBody[i];
    }
    struct Point *temp = s->snakeBody;
    free(temp);
    s->snakeBody = snakeBody2;

    struct Point snakeHead = s->snakeBody[s->size - 2];

    if ((s->lastDirection) == JOYSTICK_RIGHT) {
        struct Point newPoint = createPoint(snakeHead.x + 1, snakeHead.y);
        s->snakeBody[s->size - 1] = newPoint;
    } else if ((s->lastDirection) == JOYSTICK_LEFT) {
        struct Point newPoint = createPoint(snakeHead.x - 1, snakeHead.y);
        s->snakeBody[s->size - 1] = newPoint;
    } else if ((s->lastDirection) == JOYSTICK_UP) {
        struct Point newPoint = createPoint(snakeHead.x, snakeHead.y - 1);
        s->snakeBody[s->size - 1] = newPoint;
    } else if ((s->lastDirection) == JOYSTICK_DOWN) {
        struct Point newPoint = createPoint(snakeHead.x, snakeHead.y + 1);
        s->snakeBody[s->size - 1] = newPoint;
    }

}

void eatFruit(struct Snake *s) {
    uint8_t x = rand() % 90 + 3;
    uint8_t y = rand() % 42 + 3;

    //Wymaż z ekranu stary owoc
    oled_putPixel(s->fruit.x1, s->fruit.y1, OLED_COLOR);
    oled_putPixel(s->fruit.x2, s->fruit.y2, OLED_COLOR);
    oled_putPixel(s->fruit.x3, s->fruit.y3, OLED_COLOR);
    oled_putPixel(s->fruit.x4, s->fruit.y4, OLED_COLOR);

    struct Fruit f = createFruit(x, y);
    s->fruit = f;
    s->score += 1;
    showScore(s->score);
    incSnake(s);
    incSnake(s);

}

static void drawStruct(struct Snake *s) {

    for (int i = 0; i < s->size; i++) {
        oled_putPixel(s->snakeBody[i].x, s->snakeBody[i].y, !OLED_COLOR);
    }

    oled_putPixel(s->fruit.x1, s->fruit.y1, !OLED_COLOR);
    oled_putPixel(s->fruit.x2, s->fruit.y2, !OLED_COLOR);
    oled_putPixel(s->fruit.x3, s->fruit.y3, !OLED_COLOR);
    oled_putPixel(s->fruit.x4, s->fruit.y4, !OLED_COLOR);

    //sprawdzanie wystąpienia kolizji ze ścianami
    if (s->snakeBody[s->size - 1].x <= 1 || s->snakeBody[s->size - 1].x >= OLED_DISPLAY_WIDTH - 1 ||
        s->snakeBody[s->size - 1].y <= 1 || s->snakeBody[s->size - 1].y >= OLED_DISPLAY_HEIGHT - 1) {
        //blokowanie ruchu
        s->lastDirection = 99;
        playSong(song_up);
        s->isBlocked = TRUE;
    }

    //sprawdzenie kolizji z ciałem węża
    for (int i = 0; i < s->size - 1; i++) {
        if (s->snakeBody[s->size - 1].x == s->snakeBody[i].x) {
            if (s->snakeBody[s->size - 1].y == s->snakeBody[i].y) {
                s->lastDirection = 99;
                playSong(song_up);
                s->isBlocked = TRUE;
            }
        }
    }

    //sprawdzanie kolizji z owocami
    if ((s->snakeBody[s->size - 1].x == s->fruit.x1 && s->snakeBody[s->size - 1].y == s->fruit.y1) ||
        (s->snakeBody[s->size - 1].x == s->fruit.x2 && s->snakeBody[s->size - 1].y == s->fruit.y2) ||
        (s->snakeBody[s->size - 1].x == s->fruit.x3 && s->snakeBody[s->size - 1].y == s->fruit.y3) ||
        (s->snakeBody[s->size - 1].x == s->fruit.x4 && s->snakeBody[s->size - 1].y == s->fruit.y4)
            ) {
        eatFruit(s);
    }
}

void checkLightSensor() {
    uint32_t light = 0;
    light = light_read();
    if (light < 25) {
        if (OLED_COLOR == FALSE) {
            OLED_COLOR = TRUE;
            clearScreenWithFrame();
        }

    } else {
        if (OLED_COLOR == TRUE) {
            OLED_COLOR = FALSE;
            clearScreenWithFrame();
        }
    }
}


static void moveSnake(struct Snake *s) { //moving snake without changing direction

    if (s->lastDirection == 99)
        return;
    struct Point snakeHead = s->snakeBody[s->size - 1];

    oled_putPixel(s->snakeBody[0].x, s->snakeBody[0].y, OLED_COLOR);

    for (int i = 0; i < (s->size - 1); i++) {
        s->snakeBody[i] = s->snakeBody[i + 1];
    }
    if ((s->lastDirection) == JOYSTICK_RIGHT) {
        struct Point newPoint = createPoint(snakeHead.x + 1, snakeHead.y);
        s->snakeBody[s->size - 1] = newPoint;
    } else if ((s->lastDirection) == JOYSTICK_LEFT) {
        struct Point newPoint = createPoint(snakeHead.x - 1, snakeHead.y);
        s->snakeBody[s->size - 1] = newPoint;
    } else if ((s->lastDirection) == JOYSTICK_UP) {
        struct Point newPoint = createPoint(snakeHead.x, snakeHead.y - 1);
        s->snakeBody[s->size - 1] = newPoint;
    } else if ((s->lastDirection) == JOYSTICK_DOWN) {
        struct Point newPoint = createPoint(snakeHead.x, snakeHead.y + 1);
        s->snakeBody[s->size - 1] = newPoint;
    } else {

    }


    drawStruct(s);
}


static void init_ssp(void) {
    SSP_CFG_Type SSP_ConfigStruct;
    PINSEL_CFG_Type PinCfg;

    /*
     * Initialize SPI pin connect
     * P0.7 - SCK;
     * P0.8 - MISO
     * P0.9 - MOSI
     * P2.2 - SSEL - used as GPIO
     */
    PinCfg.Funcnum = 2;
    PinCfg.OpenDrain = 0;
    PinCfg.Pinmode = 0;
    PinCfg.Portnum = 0;
    PinCfg.Pinnum = 7;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 8;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 9;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Funcnum = 0;
    PinCfg.Portnum = 2;
    PinCfg.Pinnum = 2;
    PINSEL_ConfigPin(&PinCfg);

    SSP_ConfigStructInit(&SSP_ConfigStruct);

    // Initialize SSP peripheral with parameter given in structure above
    SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

    // Enable SSP peripheral
    SSP_Cmd(LPC_SSP1, ENABLE);

}

static void init_i2c(void) {
    PINSEL_CFG_Type PinCfg;

    /* Initialize I2C2 pin connect */
    PinCfg.Funcnum = 2;
    PinCfg.Pinnum = 10;
    PinCfg.Portnum = 0;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 11;
    PINSEL_ConfigPin(&PinCfg);

    // Initialize I2C peripheral
    I2C_Init(LPC_I2C2, 100000);

    /* Enable I2C1 operation */
    I2C_Cmd(LPC_I2C2, ENABLE);
}

void initTimer1() {
    TIM_TIMERCFG_Type TIM_ConfigStruct;
    TIM_MATCHCFG_Type TIM_MatchConfigStruct;

// Initialize timer 1, prescale count time of 1ms
    TIM_ConfigStruct.PrescaleOption = TIM_PRESCALE_USVAL;
    TIM_ConfigStruct.PrescaleValue = 1000;
    // use channel 0, MR0
    TIM_MatchConfigStruct.MatchChannel = 0;
    // Enable interrupt when MR0 matches the value in TC register
    TIM_MatchConfigStruct.IntOnMatch = FALSE;
    //Enable reset on MR0: TIMER will not reset if MR0 matches it
    TIM_MatchConfigStruct.ResetOnMatch = FALSE;
    //Stop on MR0 if MR0 matches it
    TIM_MatchConfigStruct.StopOnMatch = FALSE;
    //do no thing for external output
    TIM_MatchConfigStruct.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
    // Set Match value, count value is time (timer * 1000uS =timer mS )
    TIM_MatchConfigStruct.MatchValue = 0xfffffff;
    // Set configuration for Tim_config and Tim_MatchConfig
    TIM_Init(LPC_TIM1, TIM_TIMER_MODE, &TIM_ConfigStruct);
    TIM_ConfigMatch(LPC_TIM1, &TIM_MatchConfigStruct);
    // To start timer 1
    TIM_Cmd(LPC_TIM1, ENABLE);
}

uint32_t getTimer1TC() {
    return LPC_TIM1->TC;
}


int main(void) {

    int waitValue = 50;
    initTimer1();
    rotary_init();

    int32_t xoff = 0;
    int32_t yoff = 0;
    int32_t zoff = 0;

    int8_t x = 0;
    int8_t y = 0;
    int8_t z = 0;

    uint8_t state = 0;
    uint8_t lastJoystickState = 0;
    uint8_t lastJoystickStateCounter = 0;
    uint8_t accStateCounter = 0;
    uint8_t rotaryState = 0;
    uint8_t btn1 = 0;


    init_i2c();
    init_ssp();

    rotary_init();
    led7seg_init();
    pca9532_init();
    joystick_init();
    acc_init();
    oled_init();

    //Assume base board in zero-g position when reading first value.
    acc_read(&x, &y, &z);
    xoff = 0 - x;
    yoff = 0 - y;
    zoff = 64 - z;

    /* ---- Speaker ------> */

    GPIO_SetDir(2, 1 << 0, 1);
    GPIO_SetDir(2, 1 << 1, 1);

    GPIO_SetDir(0, 1 << 27, 1);
    GPIO_SetDir(0, 1 << 28, 1);
    GPIO_SetDir(2, 1 << 13, 1);
    GPIO_SetDir(0, 1 << 26, 1);

    GPIO_ClearValue(0, 1 << 27); //LM4811-clk
    GPIO_ClearValue(0, 1 << 28); //LM4811-up/dn
    GPIO_ClearValue(2, 1 << 13); //LM4811-shutdn

    btn1 = ((GPIO_ReadValue(0) >> 4) & 0x01);
    oled_clearScreen(!OLED_COLOR);

    struct Snake *s = createSnake(30);
    clearScreenWithFrame();
    drawStruct(s);
    showScore(s->score);
    light_enable();

    while (1) {
        /*
        Funkcjonalności:
        - Timer				- Przemek
        - OLED				- Przemek
        - GPIO (joystick)  	- Konrad
        - Czujnik światła	- Kuba
        - pca9532 (16 diód)	- Kuba
        - Akcelerometr		- Konrad
        - I2C				- Kuba
        - SSP/SPI			- Przemek
        - 0.5 - głośnik		- Konrad
        */

        rotaryState = rotary_read();

        if (rotaryState != ROTARY_WAIT) {

            if (rotaryState == ROTARY_RIGHT) {
                waitValue += 10;
            } else {
                waitValue -= 10;
            }

            if (waitValue > 125)
                waitValue = 125;
            else if (waitValue <= 5)
                waitValue = 5;

        }

        Timer0_Wait(waitValue);
        acc_read(&x, &y, &z);
        x = x + xoff;
        y = y + yoff;
        z = z + zoff;

        if (!isJoystickBroken) {
            state = joystick_read();
            if (state == JOYSTICK_LEFT || state == JOYSTICK_RIGHT || state == JOYSTICK_UP || state == JOYSTICK_DOWN) {
                if (state == lastJoystickState) {
                    lastJoystickStateCounter++;
                    if (lastJoystickStateCounter > 50) {
                        isJoystickBroken = TRUE;
                    }
                } else {
                    lastJoystickStateCounter = 0;
                }
                s->lastDirection = state;
            }
            lastJoystickState = state;
        }

        if (!isAccBroken) {
            if ((x * x + y * y + z * z) > 6000) {
                accStateCounter++;
                if (accStateCounter > 50)
                    isAccBroken = TRUE;
            } else accStateCounter = 0;
        }

        if (!isAccBroken) {
            if (x < -10) {
                state = JOYSTICK_RIGHT;
                s->lastDirection = state;
            } else if (x > 10) {
                state = JOYSTICK_LEFT;
                s->lastDirection = state;
            } else if (y > 10) {
                state = JOYSTICK_DOWN;
                s->lastDirection = state;
            } else if (y < -10) {
                state = JOYSTICK_UP;
                s->lastDirection = state;
            }

        }

        if (s->isBlocked == FALSE) {
            moveSnake(s);
        }

        btn1 = ((GPIO_ReadValue(0) >> 4) & 0x01);

        checkLightSensor();

        if (btn1 == 0) {
            playSong(song_down);

            clearScreenWithFrame();
            free(s);
            s = createSnake(30);
            s->isBlocked = FALSE;
            showScore(0);
            if (seedForSrand == 0) {
                seedForSrand = getTimer1TC();
                srand(seedForSrand);
            }
        }
        Timer0_Wait(1);
    }

    deleteOldSnake(s);


}

void check_failed(uint8_t *file, uint32_t line) {
    while (1);
}
