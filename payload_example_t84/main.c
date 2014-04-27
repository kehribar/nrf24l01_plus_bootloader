/*-----------------------------------------------------------------------------
/ “THE COFFEEWARE LICENSE” (Revision 1):
/ <ihsan@kehribar.me> wrote this file. As long as you retain this notice you
/ can do whatever you want with this stuff. If we meet some day, and you think
/ this stuff is worth it, you can buy me a coffee in return.
/----------------------------------------------------------------------------*/
#include <avr/io.h>
#include <stdint.h>
#include <avr/wdt.h>
#include "../nrf24.h"
#include "./digital.h"
#include <avr/sleep.h>
#include <avr/power.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
/*---------------------------------------------------------------------------*/
#define sbi(reg,bit) reg |= (1<<bit)
#define chbi(reg,bit) (reg&(1<<bit))
#define cbi(reg,bit) reg &= ~(1<<bit)
/*---------------------------------------------------------------------------*/
#define led0_toggle() togglePin(A,3)
#define led0_init() pinMode(A,3,OUTPUT)
#define led0_low() digitalWrite(A,3,LOW)
#define led0_high() digitalWrite(A,3,HIGH)
/*---------------------------------------------------------------------------*/
volatile uint8_t wdt_counter; 
/*---------------------------------------------------------------------------*/
uint8_t temp;
uint8_t q = 0;
uint8_t data_array[32];
uint8_t tx_address[5] = {0xE7,0xE7,0xE7,0xE7,0x00};
uint8_t rx_address[5];
/*---------------------------------------------------------------------------*/
ISR(INT0_vect)
{    
    /* one interrupt is enough! */
    cbi(GIMSK,INT0);
}
/*---------------------------------------------------------------------------*/
ISR(WDT_vect) 
{
    wdt_counter++;
}
/*---------------------------------------------------------------------------*/
uint16_t read_adc();
void init_hardware();
void init_adc(uint8_t ch);
void sleep_during_transmisson();
void sleep_for(uint8_t wdt_overflow_count);
void check_bootloader_message(uint8_t timeout);
/*---------------------------------------------------------------------------*/
int main()
{
    uint8_t i;
    uint8_t temp;    
    uint16_t rval;
    uint16_t getDataCounter;

    init_hardware();    

    while(1)
    {                                
        /* power up the temperature sensor */
        pinMode(A,2,OUTPUT);
        digitalWrite(A,2,HIGH);

        /* prepare reading the 1st ADC channel */
        init_adc(0x01);
        _delay_us(10);        

        /* take multiple readings and divide later */
        rval = 0;
        for (i = 0; i < 8; ++i)
        {
            rval += read_adc();
        }
        rval = rval >> 3;

        /* reset ADC registers */
        ADCSRA = 0x00;
        ADMUX = 0x00;

        /* power down the temperature sensor */
        digitalWrite(A,2,LOW);
        pinMode(A,2,INPUT);
        
        /* fill the data buffer */
        data_array[0] = rval >> 8;
        data_array[1] = rval & 0xFF;
        data_array[2] = 0xAA;
        data_array[3] = q++;                                            

        /* automatically goes to TX mode */
        nrf24_send(data_array);        

        /* be wise ... */
        sleep_during_transmisson();        

        /* parameter for this function determines how long does it wait for a message */
        check_bootloader_message(10);

        /* parameter for this function is WDT overflow count */
        sleep_for(25);
    }

    return 0;
}
/*---------------------------------------------------------------------------*/
void init_adc(uint8_t ch)
{
    sbi(ADCSRA,ADEN); // Enable the ADC peripheral
    
    ADMUX = ch; // Select the channel

    sbi(ADMUX,REFS1); // 1v1 internal reference

    ADCSRA |= 0x07; // Slowest prescale
}
/*---------------------------------------------------------------------------*/
uint16_t read_adc()
{
    uint16_t value;

    sbi(ADCSRA,ADSC); // Start conversion
    while(chbi(ADCSRA,ADSC)); // Wait for conversion        

    value = ADCL;       
    value += ADCH << 8; // Read the result
    
    return value;
}
/*---------------------------------------------------------------------------*/
void init_hardware()
{
    /* init interrupt pin */
    pinMode(B,2,INPUT);

    /* init led ... */
    led0_init();
    led0_low();

    /* init hardware pins */
    nrf24_init();
    
    /* disable the analog comparator */
    ACSR = (1<<ACD); 

    /* channel #2 , payload length: 4 */
    nrf24_config(2,32);

    /* get the rx address from EEPROM */
    rx_address[0] = eeprom_read_byte((uint8_t*)10);
    rx_address[1] = eeprom_read_byte((uint8_t*)11);
    rx_address[2] = eeprom_read_byte((uint8_t*)12);
    rx_address[3] = eeprom_read_byte((uint8_t*)13);
    rx_address[4] = eeprom_read_byte((uint8_t*)14);

    /* set the device addresses */
    nrf24_tx_address(tx_address);
    nrf24_rx_address(rx_address);    

    /* initial power-down */
    nrf24_powerDown();

    PRR = (1<<PRTIM1)|(1<<PRTIM0)|(0<<PRADC)|(0<<PRUSI);
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);  
    sleep_enable();
}
/*---------------------------------------------------------------------------*/
void sleep_for(uint8_t wdt_overflow_count)
{    
    /* disable timers, ADC and the USI */
    PRR = (1<<PRTIM1)|(1<<PRTIM0)|(1<<PRADC)|(1<<PRUSI);    
    
    /* set WDT parameters */
    WDTCSR = _BV (WDCE) | _BV (WDE);
    WDTCSR = _BV (WDIE) | _BV (WDP1) | _BV (WDP2); // 1 sec    
    
    /* get ready for the ISR */
    wdt_reset();    
    wdt_counter = 0;
    sei();
    
    /* go sleep until we got enough overflows! */
    while(wdt_counter < wdt_overflow_count)
    {        
        sleep_mode(); 
    }     

    /* recover the system, disable the ISR and WDT */
    cli();
    WDTCSR = 0x00;    

    /* enable the ADC and the USI peripheral, leave the timers disabled */
    PRR = (1<<PRTIM1)|(1<<PRTIM0)|(0<<PRADC)|(0<<PRUSI);        
}
/*---------------------------------------------------------------------------*/
void sleep_during_transmisson()
{
    /* sleep until the transmission over */
    sei();
    sbi(GIMSK,INT0);
    sleep_mode();
    cli();        
}
/*---------------------------------------------------------------------------*/
void check_bootloader_message(uint8_t timeout)
{
    uint8_t getDataCounter = 0;

    /* listen to any bootloader messages! */
    nrf24_powerUpRx();    
    while(getDataCounter++ < timeout)
    {
        _delay_ms(1);        
        if(nrf24_dataReady())
        {    
            nrf24_getData(data_array);        
            if((data_array[0] == 0) && (data_array[1] == 0xAA))
            {        
                /* prepare */
                cli();
                PORTA = 0x00;
                PORTB = 0x00;
                DDRA = 0x00;
                DDRB = 0x00;
                sleep_disable();

                /* jump to bootloader! */
                /* bootloader is located at: 0x1800 */
                asm volatile("lds r31, 0x18"); // R31 = ZH
                asm volatile("lds r30, 0x00"); // R30 = ZL
                asm volatile("icall"); // Jump to the Z register value
            }            
        }                            
    }        
    nrf24_powerDown();
}
/*---------------------------------------------------------------------------*/