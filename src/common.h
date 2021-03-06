#ifndef COMMON_H
#define COMMON_H

#ifndef __AVR_ATmega16A__
    #define __AVR_ATmega16A__
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/atomic.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdio.h>

#include "ul_rbuf.h"
#include "pt.h"
#include "timer.h"

#include "pin_macros.h"
#include "pins.h"

#include "heater.h"

#define VERSION "0.1"


//#define PSTRX(x, y) static const char x[] PROGMEM = (y)
#define PGM(a) extern const a PROGMEM; const a
#define PGMSTR(name,value) \
    extern const char name[] PROGMEM; const char name[] = (value)



#define PN_PIN_(port,bit,val) (PIN##port)
#define PN_PORT_(port,bit,val) (PORT##port)
#define PN_DDR_(port,bit,val) (DDR##port)

#define PN_PIN(x) PN_PIN_(x)
#define PN_PORT(x) PN_PORT_(x)
#define PN_DDR(x) PN_DDR_(x)


#define AVR_RESET do { cli(); wdt_reset(); wdt_enable( WDTO_15MS ); while(1); } while (0)

#define _ON  1
#define _OFF 0


//действия
typedef enum { ACT_NONE, ACT_PUSH, ACT_PUSH_LONG, ACT_ROTATE_LEFT, ACT_ROTATE_RIGHT } TActions;

//кнопки и энкодер
typedef enum { NM_NONE, NM_BUTTON1, NM_BUTTON2, NM_BUTTON3, NM_BUTTON4,
                    NM_ENCBUTTON, NM_ENCROTATE, NM_BUTTON1_ENC } TActElements;

//список элементов в меню
//uses for g_ui_menu
typedef enum { MENU_SELECT, MENU_IRON, MENU_FEN, MENU_DREL } TMenuStates;



typedef struct {
    TMenuStates menu;
    uint8_t update_screen;

    uint8_t temp; //для временного юзания

    THeater iron; //параметры паяльника
    THeater fen; //параметры фена

    volatile THeater *heater;

} TGlobalData;



extern volatile TGlobalData g_data;


#endif // COMMON_H
