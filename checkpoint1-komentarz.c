//Ten plik zawiera opisy funkcji użytych w pliku main.c
//Nie należy go dodwać do projektu, budować ani kompilować, służy wyłącznie jako komentarz

#define JOYSTICK_CENTER 0x01
#define JOYSTICK_UP     0x02
#define JOYSTICK_DOWN   0x04
#define JOYSTICK_LEFT   0x08
#define JOYSTICK_RIGHT  0x10


uint8_t joystick_read(void) //funkcja zwracająca stan interakcjii z joystickiem
{                           //stan joysticka to 8 bitowa liczba bez znaku

    uint8_t status = 0;     //inicjalizacja zmiennej do zwrócenia
    uint32_t port0 = 0;
    uint32_t port2 = 0;
                            //GPIO -  Wejście-wyjście ogólnego przeznaczenia
    GPIO_ReadValue(0);      //sczytanie wartości z wzkazanego przez argument pinu
    port0 = GPIO_ReadValue(0);
    port2 = GPIO_ReadValue(2);


    //użycie |= (alternatywa z aktualną wartościa i zapis do tej wartości) pozwala na odczytanie dwóch stanów jednocześnie
    //np. joystick może być przesunięty w róg

    if (/*!GPIOGetValue( PORT2, 0)*/(port0 & (1 << 17)) == 0) {
        status |= JOYSTICK_CENTER;
    }

    if (/*!GPIOGetValue( PORT2, 1)*/ (port0 & (1 << 15)) == 0) {
        status |= JOYSTICK_DOWN;
    }

    if (/*!GPIOGetValue( PORT2, 2)*/ (port0 & (1 << 16)) == 0) {
        status |= JOYSTICK_RIGHT;
    }

    if (/*!GPIOGetValue( PORT2, 3)*/ (port2 & (1 << 3)) == 0) {
        status |= JOYSTICK_UP;
    }


    if (/*!GPIOGetValue( PORT2, 4)*/ (port2 & (1 << 4)) == 0) {
        status |= JOYSTICK_LEFT;
    }

    return status;
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



static void playNote(uint32_t note, uint32_t durationMs) { //zagranie tonu to pobudzenie głośnika z częstotliwością odpowiednią dla dźwięku

    uint32_t t = 0;

    if (note > 0) {

        while (t < (durationMs*1000)) {
            NOTE_PIN_HIGH();        // #define NOTE_PIN_HIGH() GPIO_SetValue(0, 1<<26);
                                    // pobudzenie pinu
            Timer0_us_Wait(note / 2);
            //delay32Us(0, note / 2);

            NOTE_PIN_LOW();        //#define NOTE_PIN_LOW()  GPIO_ClearValue(0, 1<<26);
                                   //wyczyszczenie wartosci
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


static uint8_t * song_center = (uint8_t*)"C2.C2,D4,C4,F4,"; //C, D, F, ... - oznaczenie (nazwa) dźwięku w notacji muzycznej
                                                            //2, 3, 4, ... - czas trwania dźwięku w milisekundach podzielony przez 200
                                                            //znak odpowiadajacy czasu pauzy
                                                            //+ - 0 ms
                                                            //, - 5 ms
                                                            //. - 20 ms
                                                            //_ - 30ms