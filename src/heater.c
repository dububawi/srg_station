#include "common.h"
#include "hal.h"
#include "uart.h"
#include "ui.h"
#include "heater.h"

typedef struct {
    uint8_t header;

    uint8_t type;
    uint16_t value1;
    uint16_t value2;
    uint16_t value3;
    uint16_t value4;

    uint8_t crc;
} TPCInfo;

#define PCINFO_HEADER 0xDE
#define PCINFO_TYPE_IRON 0x01

#define PCINFO_TYPE_PRINT 0x05

#include <util/crc16.h>
void send_uart_info(TPCInfo *info) {
    info->header = PCINFO_HEADER;

    uint8_t i, crc = 0;
    uint8_t *p = (uint8_t*)info;

    for(i = 0; i < sizeof(TPCInfo) - 1; i++)
        crc = _crc_ibutton_update(crc, p[i]);

    info->crc = crc;

    for(i = 0; i < sizeof(TPCInfo); i++)
        uart_send_b(p[i]);
}

TTempZones gIronTempZones[] = {
    TZ_X(TZ_XY0, TZ_XY1),
    TZ_X(TZ_XY1, TZ_XY2),
    TZ_X(TZ_XY2, TZ_XY3),
    TZ_X(TZ_XY3, TZ_XY4),
    TZ_X(TZ_XY4, TZ_XY5),
    TZ_X(TZ_XY5, TZ_XY6),
};


//читалка adc c пина
uint16_t adc_read(uint8_t adc_pin)
{
    ADMUX = (ADMUX & 0b011111000) | adc_pin;
    _delay_us(125);

    ADCSRA |= _BV(ADSC);         // start single convertion
    loop_until_bit_is_set(ADCSRA, ADSC); // Wait for the AD conversion to complete

    uint16_t temp;

    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        temp = ADCW;
    }
    return temp;
}

//конверт adc в темп по калиброванным значениям
uint16_t find_temp(uint16_t adc, const TTempZones* tempzones, uint8_t count) {
    uint8_t i;
    for(i = 0; i < count; i++ ) {
        if(adc <= tempzones[i].y1)
            break;
    }

    uint16_t temp = ((adc - tempzones[i].y0) * tempzones[i].a) / TZ_AMUL +
            tempzones[i].x0;

    return temp;
}


volatile uint8_t pid_p = IRON_PID_KP;
volatile uint8_t pid_i = IRON_PID_KI;
volatile uint8_t pid_d = IRON_PID_KD;
volatile uint8_t send_stat = 1;

static volatile double pre_error = 0;
static volatile double integral = 0;

void pid_init(void) {
    pre_error = integral = 0;
}

#include <stdlib.h>
#include <math.h>
uint8_t pid_Controller(uint16_t temp_need, uint16_t temp_curr) {
    double error, out;

    error = (int16_t)(temp_need - temp_curr);

    integral += error * (IRON_PID_DELTA_T / 1000.0);

    out = (1.0 / pid_d) * (
        error +
        (1.0 / pid_i) * integral +
        pid_d * (error - pre_error) / (IRON_PID_DELTA_T / 1000.0)
    );

    pre_error = error;

    if(temp_curr < IRON_TEMP_SOFT && error > 0) {
        return POWER_MAX / 4;
    }

    if(out < 0.0)
        out = 0;
    else
    if(out > 1.0) {
        out = 1;
    }
    else out = ceil(out);

    return 100 * (uint8_t)out;
}

/*
void heater_iron_setpower(uint16_t pow) {
    g_data.iron.power = pow;

    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        OCR1A = pgm_read_word(&gPowerMas[pow]);
    }
}
*/
void heater_iron_setpower(uint16_t pow) {
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        //g_data.iron.power = pow;

        //g_data.iron.sigma = POWER_MAX;
    }
}

void heater_iron_on(void) {
    if(g_data.iron.on == _ON) return;

    memset((void*) &g_data.iron, 0, sizeof(TIron));

    g_data.iron.temp_need = IRON_TEMP_MIN;

    g_data.iron.on = _ON;
}

void heater_iron_off(void) {
    if(g_data.iron.on == _OFF) return;

    heater_iron_setpower(0);

    g_data.iron.on = _OFF;
}

void heater_fen_on(void) {
    //if(g_data.fen.on  == _ON) return;

}

void heater_fen_off(void) {
    //if(g_data.fen.on == _OFF) return;

}

PT_THREAD(iron_pt_manage(struct pt *pt)) {
    static TIMER_T timer;

    PT_BEGIN(pt);

    TIMER_INIT(timer, IRON_PID_DELTA_T);
    for(;;) {
        PT_WAIT_UNTIL(pt, g_data.iron.on == _ON && TIMER_ENDED(timer));
        TIMER_INIT(timer, IRON_PID_DELTA_T);

        volatile TIron *iron = &g_data.iron;

        uint16_t adc = adc_read(ADC_PIN_IRON);

        if(0&&adc > 850) BEEP(100);

        if(0&&adc >= IRON_ADC_ERROR) {
            heater_iron_setpower(0);

            ui_set_update_screen(UPDATE_SCREEN_ERROR);
            continue;
        }

        if(1||adc != iron->adc) {
            iron->adc = adc;

            //iron->temp = find_temp(adc, gIronTempZones, sizeof(gIronTempZones));

            if(iron->temp > iron->temp_need)
                iron->temp --;
            else
                if(iron->temp < iron->temp_need)
                    iron->temp ++;


            ui_set_update_screen(UPDATE_SCREEN_VALS);
        }

        uint8_t pow;

        //pow = pid_Controller(iron->temp_need, iron->temp);
        pow = iron->power;

        if(1 || pow != iron->power) {

            heater_iron_setpower(pow);
            ui_set_update_screen(UPDATE_SCREEN_VALS);
        }

        if(send_stat) {
            TPCInfo info;

            info.type = PCINFO_TYPE_IRON;

            info.value1 = iron->temp;
            info.value2 = iron->power;
            info.value3 = iron->temp_need;
            info.value4 = iron->adc;

            send_uart_info(&info);

        }
    }

    PT_END(pt);
}


PT_THREAD(fen_pt_manage(struct pt *pt)) {
    PT_BEGIN(pt);
    PT_END(pt);
}


PT_THREAD(heater_pt_manage(struct pt *pt)) {
    static struct pt pt_iron, pt_fen;

    PT_BEGIN(pt);

    PT_INIT(&pt_iron);
    PT_INIT(&pt_fen);

    PT_WAIT_THREAD(pt,
           iron_pt_manage(&pt_iron) &
           fen_pt_manage(&pt_fen)
          );

    PT_END(pt);
}


void heater_init_mod(void) {

    heater_iron_off();
    heater_fen_off();
}


//ZCD
ISR(INT2_vect) {

#if 0
    static uint8_t numperiod = 0;
    if(numperiod++ & 1) return;
#endif

    if(g_data.iron.on == _ON) {
         int8_t delta;

         if(g_data.iron.sigma > POWER_MAX) {
            delta = -POWER_MAX;

            TCNT1 = 0x00;
            TCCR1B |= TIMER1A_PRESCALE; //вкл таймер 1
        } else {
            delta = 0;
        }

        g_data.iron.sigma += g_data.iron.power + delta;
    }
}

ISR(TIMER1_COMPA_vect) {

    if(!ACTIVE(P_IRON_PWM))
        ON(P_IRON_PWM); //вкл симмистор
    else {
        OFF(P_IRON_PWM);
        TCCR1B &= ~TIMER1A_PRESCALE_OFF; //выкл таймер 1
    }
}
