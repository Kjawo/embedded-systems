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

#include <stdlib.h>

static uint8_t barPos = 2;

#define NOTE_PIN_HIGH() GPIO_SetValue(0, 1<<26);
#define NOTE_PIN_LOW()  GPIO_ClearValue(0, 1<<26);


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

static void playNote(uint32_t note, uint32_t durationMs) {

    uint32_t t = 0;

    if (note > 0) {

        while (t < (durationMs*1000)) {
            NOTE_PIN_HIGH();
            Timer0_us_Wait(note / 2);
            //delay32Us(0, note / 2);

            NOTE_PIN_LOW();
            Timer0_us_Wait(note / 2);
            //delay32Us(0, note / 2);

            t += note;
        }

    }
    else {
    	Timer0_Wait(durationMs);
        //delay32Ms(0, durationMs);
    }
}

static uint32_t getNote(uint8_t ch)
{
    if (ch >= 'A' && ch <= 'G')
        return notes[ch - 'A'];

    if (ch >= 'a' && ch <= 'g')
        return notes[ch - 'a' + 7];

    return 0;
}

static uint32_t getDuration(uint8_t ch)
{
    if (ch < '0' || ch > '9')
        return 400;

    /* number of ms */

    return (ch - '0') * 200;
}

static uint32_t getPause(uint8_t ch)
{
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
    uint32_t dur  = 0;
    uint32_t pause = 0;

    /*
     * A song is a collection of tones where each tone is
     * a note, duration and pause, e.g.
     *
     * "E2,F4,"
     */

    while(*song != '\0') {
        note = getNote(*song++);
        if (*song == '\0')
            break;
        dur  = getDuration(*song++);
        if (*song == '\0')
            break;
        pause = getPause(*song++);

        playNote(note, dur);
        //delay32Ms(0, pause);
        Timer0_Wait(pause);

    }
}

static uint8_t * song_up = (uint8_t*)"C4,";
static uint8_t * song_down = (uint8_t*)"D4,";
static uint8_t * song_left = (uint8_t*)"E4,";
static uint8_t * song_right = (uint8_t*)"F4,";
static uint8_t * song_center = (uint8_t*)"C2.C2,D4,C4,F4,";
static uint8_t * song = (uint8_t*)"C2.C2,D4,C4,F4,E8,";

static void moveBar(uint8_t steps, uint8_t dir)
{
    uint16_t ledOn = 0;

    if (barPos == 0)
        ledOn = (1 << 0) | (3 << 14);
    else if (barPos == 1)
        ledOn = (3 << 0) | (1 << 15);
    else
        ledOn = 0x07 << (barPos-2);

    barPos += (dir*steps);
    barPos = (barPos % 16);

    pca9532_setLeds(ledOn, 0xffff);
}

static uint8_t ch7seg = '0';
static void change7Seg(uint8_t rotaryDir)
{

    if (rotaryDir != ROTARY_WAIT) {

        if (rotaryDir == ROTARY_RIGHT) {
            ch7seg++;
        }
        else {
            ch7seg--;
        }

        if (ch7seg > '9')
            ch7seg = '0';
        else if (ch7seg < '0')
            ch7seg = '9';

        led7seg_setChar(ch7seg, FALSE);

    }
}

static void drawOled(uint8_t joyState)
{
    static int wait = 0;
    static uint8_t currX = 48;
    static uint8_t currY = 32;
    static uint8_t lastX = 0;
    static uint8_t lastY = 0;

    if ((joyState & JOYSTICK_CENTER) != 0) {
        oled_clearScreen(OLED_COLOR_BLACK);
        return;
    }

    if (wait++ < 3)
        return;

    wait = 0;

    if ((joyState & JOYSTICK_UP) != 0 && currY > 0) {
        currY--;
    }

    if ((joyState & JOYSTICK_DOWN) != 0 && currY < OLED_DISPLAY_HEIGHT-1) {
        currY++;
    }

    if ((joyState & JOYSTICK_RIGHT) != 0 && currX < OLED_DISPLAY_WIDTH-1) {
        currX++;
    }

    if ((joyState & JOYSTICK_LEFT) != 0 && currX > 0) {
        currX--;
    }

    if (lastX != currX || lastY != currY) {
        oled_putPixel(currX, currY, OLED_COLOR_WHITE);
        lastX = currX;
        lastY = currY;
    }
}

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
	struct Point* snakeBody;
	struct Fruit fruit;
	uint8_t size;
	uint8_t lastDirection;
};



struct Point createPoint(int x, int y) {
		//uint8_t snakeB[] = {10,32,11,32,12,32,13,32,14,32,15,32};
	struct Point point;
	point.x = x;
	point.y = y;
	return point;
}

void clearScreenWithFrame() {
	oled_clearScreen(OLED_COLOR_BLACK);
  	for(int i = 0; i < OLED_DISPLAY_HEIGHT-1; i++) {
   		oled_putPixel(0, i, OLED_COLOR_WHITE);
   		oled_putPixel(OLED_DISPLAY_WIDTH-1, i, OLED_COLOR_WHITE);
   	}

   	for(int i = 0; i < OLED_DISPLAY_WIDTH-1; i++) {
   	   		oled_putPixel(i, 0, OLED_COLOR_WHITE);
   	   	oled_putPixel(i, OLED_DISPLAY_HEIGHT-1, OLED_COLOR_WHITE);
   	   	}
}


struct Snake *createSnake(uint8_t size) {
	//uint8_t size = 5;
	//uint8_t snakeB[] = {10,32,11,32,12,32,13,32,14,32,15,32};
	struct Point* snakeBody;
	snakeBody = malloc(size * sizeof(struct Point));
	for(int i = 0; i < size; i++) {
		struct Point p = createPoint(48+i, 50);
		snakeBody[i] = p;
	}
	struct Snake *newSnake;
	newSnake = malloc(sizeof(struct Snake));
	newSnake->size = size;
	newSnake->snakeBody = snakeBody;
	newSnake->lastDirection = JOYSTICK_UP;
	struct Fruit f;
	f.x1 = 30;
	f.y1 = 30;

	f.x2 = f.x1 + 1;
	f.y2 = f.y1;

	f.x3 = f.x1;
	f.y3 = f.y1 + 1;

	f.x4 = f.x1 + 1;
	f.y4 = f.y1 + 1;

	newSnake->fruit = f;

	return newSnake;
}

void deleteSnake(struct Snake *s) {
//	for(int i = 0; i < s->size; i++) {
//		free(s->snakeBody[i].x);
//		free(s->snakeBody[i].y);
//	}
//	free(s->snakeBody);
	//free(s->lastDirection);
//	free(s);
	int size = 30;
	s->size = size;
	struct Point* snakeBody;
		snakeBody = malloc(size * sizeof(struct Point));
		for(int i = 0; i < size; i++) {
			struct Point p = createPoint(48+i, 32);
			snakeBody[i] = p;
		}
//	free(s->snakeBody);
	s->snakeBody = snakeBody;
	clearScreenWithFrame();
}

//napisac funkcje dla stuktury Snake: resize, push, pop, drawStruct, checkCollision

static void drawStruct(struct Snake *s) {
	if(s->snakeBody[s->size - 1].x < 0 || s->snakeBody[s->size - 1].x > OLED_DISPLAY_WIDTH || s->snakeBody[s->size - 1].y < 0 || s->snakeBody[s->size - 1].y > OLED_DISPLAY_HEIGHT) {
		s->lastDirection = 99;
		playSong(song_up);
	}

	for(int i = 0; i < s->size - 1; i++) {
		if(s->snakeBody[s->size - 1].x == s->snakeBody[i].x) {
			if(s->snakeBody[s->size - 1].y == s->snakeBody[i].y) {
				s->lastDirection = 99;
				playSong(song_up);
			}
		}
	}



	for(int i = 0; i < s->size; i++) {
		oled_putPixel(s->snakeBody[i].x, s->snakeBody[i].y, OLED_COLOR_WHITE);
	}

	oled_putPixel(s->fruit.x1, s->fruit.y1, OLED_COLOR_WHITE);
	oled_putPixel(s->fruit.x2, s->fruit.y2, OLED_COLOR_WHITE);
	oled_putPixel(s->fruit.x3, s->fruit.y3, OLED_COLOR_WHITE);
	oled_putPixel(s->fruit.x4, s->fruit.y4, OLED_COLOR_WHITE);

}

static void moveSnake(struct Snake *s, uint8_t joyState) { //moving snake without changing direction
	//struct Point* newBody;
	//newBody = malloc(s->size * sizeof(struct Point));
	if(s->lastDirection == 99)
			return;
	struct Point snakeHead = s->snakeBody[s->size - 1];
	struct Point snakeTail = s->snakeBody[0];


	oled_putPixel(s->snakeBody[0].x, s->snakeBody[0].y, OLED_COLOR_BLACK);



	for(int i = 0; i < (s->size - 1); i++) {
		s->snakeBody[i] = s->snakeBody[i + 1];
	}
	//struct Point newPoint = createPoint(lastPoint.x + 1, lastPoint.y);
	//s->snakeBody[s->size - 1] = newPoint;
	if((s->lastDirection) == JOYSTICK_RIGHT) {
		struct Point newPoint = createPoint(snakeHead.x + 1, snakeHead.y);
		s->snakeBody[s->size - 1] = newPoint;
//		oled_clearScreen(OLED_COLOR_BLACK);

//		drawStruct(s);
	} else if((s->lastDirection) == JOYSTICK_LEFT) {
			struct Point newPoint = createPoint(snakeHead.x - 1, snakeHead.y);
			s->snakeBody[s->size - 1] = newPoint;
//			oled_clearScreen(OLED_COLOR_BLACK);
//			drawStruct(s);
		} else if((s->lastDirection) == JOYSTICK_UP) {
			struct Point newPoint = createPoint(snakeHead.x, snakeHead.y-1);
			s->snakeBody[s->size - 1] = newPoint;
//			oled_clearScreen(OLED_COLOR_BLACK);
//			drawStruct(s);
		} else if((s->lastDirection) == JOYSTICK_DOWN) {
		struct Point newPoint = createPoint(snakeHead.x, snakeHead.y+1);
		s->snakeBody[s->size - 1] = newPoint;
//		oled_clearScreen(OLED_COLOR_BLACK);s
//		drawStruct(s);
	} else {

	}


	drawStruct(s);
}

static void drawSnake(uint8_t joyState, uint8_t *lastDirection)
{
    static int wait = 0;
    static uint8_t currX = 48;
    static uint8_t currY = 32;
    static uint8_t lastX = 0;
    static uint8_t lastY = 0;
    uint8_t currDirection = 0;

    if ( joyState != 0 ) {
    	currDirection = joyState;
    } else {
    	currDirection = *lastDirection;
    }

    if ((joyState & JOYSTICK_CENTER) != 0) {
        oled_clearScreen(OLED_COLOR_BLACK);
        return;
    }

    if (wait++ < 3)
        return;

    wait = 0;

    if ((currDirection & JOYSTICK_UP) != 0 && currY > 0) {
        currY--;
    }

    if ((currDirection & JOYSTICK_DOWN) != 0 && currY < OLED_DISPLAY_HEIGHT-1) {
        currY++;
    }

    if ((currDirection & JOYSTICK_RIGHT) != 0 && currX < OLED_DISPLAY_WIDTH-1) {
        currX++;
    }

    if ((currDirection & JOYSTICK_LEFT) != 0 && currX > 0) {
        currX--;
    }

    if (lastX != currX || lastY != currY) {
        oled_putPixel(currX, currY, OLED_COLOR_WHITE);
        lastX = currX;
        lastY = currY;
    }
    *lastDirection = currDirection;
}

static void changeRgbLeds(uint32_t value)
{
    uint8_t leds = 0;

    leds = value / 128;

    rgb_setLeds(leds);
}








        //(uint8_t*)"C2.C2,D4,C4,F4,E8,C2.C2,D4,C4,G4,F8,C2.C2,c4,A4,F4,E4,D4,A2.A2,H4,F4,G4,F8,";
        //"D4,B4,B4,A4,A4,G4,E4,D4.D2,E4,E4,A4,F4,D8.D4,d4,d4,c4,c4,B4,G4,E4.E2,F4,F4,A4,A4,G8,";



static void init_ssp(void)
{
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

static void init_i2c(void)
{
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

static void init_adc(void)
{
	PINSEL_CFG_Type PinCfg;

	/*
	 * Init ADC pin connect
	 * AD0.0 on P0.23
	 */
	PinCfg.Funcnum = 1;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Pinnum = 23;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);

	/* Configuration for ADC :
	 * 	Frequency at 0.2Mhz
	 *  ADC channel 0, no Interrupt
	 */
	ADC_Init(LPC_ADC, 200000);
	ADC_IntConfig(LPC_ADC,ADC_CHANNEL_0,DISABLE);
	ADC_ChannelCmd(LPC_ADC,ADC_CHANNEL_0,ENABLE);

}


int main (void) {


    int32_t xoff = 0;
    int32_t yoff = 0;
    int32_t zoff = 0;

    int8_t x = 0;

    int8_t y = 0;
    int8_t z = 0;
    uint8_t dir = 1;
    uint8_t wait = 0;

    uint8_t state    = 0;

    uint32_t trim = 0;

    uint8_t btn1 = 0;


    init_i2c();
    init_ssp();
    init_adc();

    rotary_init();
    led7seg_init();

    pca9532_init();
    joystick_init();
    acc_init();
    oled_init();
    rgb_init();


    /*
     * Assume base board in zero-g position when reading first value.
     */
    acc_read(&x, &y, &z);
    xoff = 0-x;
    yoff = 0-y;
    zoff = 64-z;

    /* ---- Speaker ------> */

    GPIO_SetDir(2, 1<<0, 1);
    GPIO_SetDir(2, 1<<1, 1);

    GPIO_SetDir(0, 1<<27, 1);
    GPIO_SetDir(0, 1<<28, 1);
    GPIO_SetDir(2, 1<<13, 1);
    GPIO_SetDir(0, 1<<26, 1);

    GPIO_ClearValue(0, 1<<27); //LM4811-clk
    GPIO_ClearValue(0, 1<<28); //LM4811-up/dn
    GPIO_ClearValue(2, 1<<13); //LM4811-shutdn

    btn1 = ((GPIO_ReadValue(0) >> 4) & 0x01);

    /* <---- Speaker ------ */

    moveBar(1, dir);
    oled_clearScreen(OLED_COLOR_BLACK);


    struct Snake *s = createSnake(30);
   	drawStruct(s);
   	clearScreenWithFrame();

    while (1) {

        /* ####### Accelerometer and LEDs  ###### */
        /* # */
//    	moveSnake(s);
//    	if(i == 5) s->lastDirection = JOYSTICK_DOWN;
//    	if(i == 10) s->lastDirection = JOYSTICK_RIGHT;
//    	if(i == 15) s->lastDirection = JOYSTICK_UP;
    	Timer0_Wait(50);
        acc_read(&x, &y, &z);
        x = x+xoff;
        y = y+yoff;
        z = z+zoff;

        if (y < 0) {
            dir = 1;
            y = -y;
        }
        else {
            dir = -1;
        }

        y *= 5;
        if (y > 1 && wait++ > (40 / (1 + (y/10)))) {
            moveBar(1, dir);
            wait = 0;
        }

        /* # */
        /* ############################################ */


        /* ####### Rotary and 7-segment display  ###### */
        /* # */

        change7Seg(rotary_read());

        /* # */
        /* ############################################# */


        /* ####### Joystick and OLED  ###### */
        /* # */
        state = joystick_read();
        if (state == JOYSTICK_LEFT || state == JOYSTICK_RIGHT || state == JOYSTICK_UP || state == JOYSTICK_DOWN) {
        	 s->lastDirection = state;
//        	 moveSnake(s);
        }
        moveSnake(s, state);
        //drawSnake(state, lastDirectionPtr);


       // oled_putPixel(currX, currY, OLED_COLOR_WHITE);


        /* # */
        /* ############################################# */

        btn1 = ((GPIO_ReadValue(0) >> 4) & 0x01);

        /* ############ Trimpot and RGB LED  ########### */
        /* # */


		ADC_StartCmd(LPC_ADC,ADC_START_NOW);
		//Wait conversion complete
		while (!(ADC_ChannelGetStatus(LPC_ADC,ADC_CHANNEL_0,ADC_DATA_DONE)));
		trim = ADC_ChannelGetData(LPC_ADC,ADC_CHANNEL_0);

        changeRgbLeds(trim);


        /* # */
        /* ############################################# */



        if (btn1 == 0) {
        	playSong(song_down);
        	clearScreenWithFrame();
        	free(s);
        	s = createSnake(30);
        }


        Timer0_Wait(1);
    }

    deleteSnake(s);


}

void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while(1);
}
