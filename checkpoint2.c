//Plik z opisami funkcji do checkpoint2

oid oled_init (void) // inicjalizacja wyswietlacza
{
    int i = 0;

    //GPIO_SetDir(PORT0, 0, 1);
    GPIO_SetDir(2, (1<<1), 1); //ustawienie portow na wyjscie
    GPIO_SetDir(2, (1<<7), 1);
    GPIO_SetDir(0, (1<<6), 1);

    /* make sure power is off */
    GPIO_ClearValue( 2, (1<<1) );

#ifdef OLED_USE_I2C
    GPIO_ClearValue( 2, (1<<7)); // D/C#
    GPIO_ClearValue( 0, (1<<6)); // CS#
#else
    OLED_CS_OFF(); // = GPIO_SetValue( 0, (1<<6) )
#endif

    runInitSequence(); //wysyla okreslone infromacje przy pomocy wrtieCommand przez SSP

    memset(shadowFB, 0, SHADOW_FB_SIZE); // shadow framebuffer

    /* small delay before turning on power */
    for (i = 0; i < 0xffff; i++);

     /* power on */
    GPIO_SetValue( 2, (1<<1) );
}

static void
writeCommand(uint8_t data)
{

#ifdef OLED_USE_I2C
    uint8_t buf[2];


    buf[0] = 0x00; // write Co & D/C bits
    buf[1] = data; // data

    I2CWrite(OLED_I2C_ADDR, buf, 2);

#else
    SSP_DATA_SETUP_Type xferConfig;
    OLED_CMD(); //  GPIO_ClearValue( 2, (1<<7) )
    OLED_CS_ON(); // GPIO_ClearValue( 0, (1<<6) )

	xferConfig.tx_data = &data;
	xferConfig.rx_data = NULL;
	xferConfig.length  = 1;

    SSP_ReadWrite(LPC_SSP1, &xferConfig, SSP_TRANSFER_POLLING);
    //SSPSend( (uint8_t *)&data, 1 );

    OLED_CS_OFF(); // GPIO_SetValue( 0, (1<<6) )
#endif
}

void oled_clearScreen(oled_color_t color) // czysci wyswietlacz wysylajac jeden kolor
{
    uint8_t i;
    uint8_t c = 0;

    if (color == OLED_COLOR_WHITE)
        c = 0xff;


    for(i=0xB0;i<0xB8;i++) {            // Go through all 8 pages
        setAddress(i,0x00,0x10); // Set the address (sets the page,
        writeDataLen(c, 132);   // lower and higher column address pointers)
    }

    memset(shadowFB, c, SHADOW_FB_SIZE);
}


void oled_putPixel(uint8_t x, uint8_t y, oled_color_t color) {
    uint8_t page;
    uint16_t add;
    uint8_t lAddr;
    uint8_t hAddr;
    uint8_t mask;
    uint32_t shadowPos = 0;

    if (x > OLED_DISPLAY_WIDTH) {
        return;
    }
    if (y > OLED_DISPLAY_HEIGHT) {
        return;
    }

    /* page address */
         if(y < 8)  page = 0xB0;
    else if(y < 16) page = 0xB1;
    else if(y < 24) page = 0xB2;
    else if(y < 32) page = 0xB3;
    else if(y < 40) page = 0xB4;
    else if(y < 48) page = 0xB5;
    else if(y < 56) page = 0xB6;
    else            page = 0xB7;

    add = x + X_OFFSET;
    lAddr = 0x0F & add;             // Low address
    hAddr = 0x10 | (add >> 4);      // High address

    // Calculate mask from rows basically do a y%8 and remainder is bit position
    add = y>>3;                     // Divide by 8
    add <<= 3;                      // Multiply by 8
    add = y - add;                  // Calculate bit position
    mask = 1 << add;                // Left shift 1 by bit position

    setAddress(page, lAddr, hAddr); // Set the address (sets the page,
                                    // lower and higher column address pointers)

    shadowPos = (page-0xB0)*OLED_DISPLAY_WIDTH+x;

    if(color > 0)
        shadowFB[shadowPos] |= mask;
    else
        shadowFB[shadowPos] &= ~mask;


    writeData(shadowFB[shadowPos]);
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
