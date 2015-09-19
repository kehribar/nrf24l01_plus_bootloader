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
#include <avr/eeprom.h>
#include "./bitbang_i2c.h"
#include <avr/interrupt.h>
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
const uint8_t tx_address[5] = {0xE7,0xE7,0xE7,0xE7,0x00};
const uint8_t rx_address[5] = {0xE7,0xE7,0xE7,0xE7,0xFF};
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
void init_hardware();
void sleep_during_transmisson();
void sleep_for(uint8_t wdt_overflow_count);
/*---------------------------------------------------------------------------*/
int main()
{
    uint16_t i;
    uint8_t rv;
    uint8_t adc_addr;
    uint8_t temp = 0;
    uint8_t data_array[32];

    init_hardware();        

    /* Power up the ADC */
    pinMode(A,2,OUTPUT);
    digitalWrite(A,2,HIGH);
    _delay_ms(10);

    /* Only single device attached to the bus ... */
    for(i=1;i<128;i++)
    {
        I2C_Start();
        rv = I2C_Write(write_address(i));
        I2C_Stop();

        if(rv == 0)
        {
            adc_addr = i;
        }
    }

    /* Clear the transmission buffer */
    memset(data_array,0x00,32);
    
    while(1)
    {           
        /*-------------------------------------------------------------------*/
        /* MCP3421: 3.75 SPS + 18 Bits + Initiate new conversion
        /*-------------------------------------------------------------------*/
        I2C_Start();
        I2C_Write(write_address(adc_addr));
        I2C_Write((1<<7)|(1<<3)|(1<<2));
        I2C_Stop();

        /* Sleep during conversion */        
        sleep_for(1);

        /* Get the reading from the ADC */
        I2C_Start();
        I2C_Write(read_address(adc_addr));
        data_array[0] = I2C_Read(ACK);
        data_array[1] = I2C_Read(ACK);
        data_array[2] = I2C_Read(NO_ACK);
        I2C_Stop();
        
        /* Counter */
        data_array[31] = temp++;
        
        /* Automatically goes to TX mode */
        nrf24_send(data_array);        

        /* Be wise ... */
        sleep_during_transmisson();
        nrf24_powerDown(); 
        
        /* Sleep some time more ... */
        sleep_for(1);
    }

    return 0;
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

    /* channel #2 , payload length: 32 */
    nrf24_config(2,32);

    /* set the device addresses */
    nrf24_tx_address(tx_address);
    nrf24_rx_address(rx_address);    

    /* initial power-down */
    nrf24_powerDown();

    PRR = (1<<PRTIM1)|(1<<PRTIM0)|(1<<PRADC)|(0<<PRUSI);

    I2C_Init();
}
/*---------------------------------------------------------------------------*/
void sleep_for(uint8_t wdt_overflow_count)
{    
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);  
    sleep_enable();

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

    /* enable the USI peripheral */
    PRR = (1<<PRTIM1)|(1<<PRTIM0)|(1<<PRADC)|(0<<PRUSI);        
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
