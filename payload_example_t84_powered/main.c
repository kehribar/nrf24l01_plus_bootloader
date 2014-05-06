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
uint8_t data_array[32];
uint8_t rx_address[5]; /* read this from EEPROM */
uint8_t tx_address[5] = {0xE7,0xE7,0xE7,0xE7,0x00};
/*---------------------------------------------------------------------------*/
void led_effect();
uint8_t direction = 1;
uint16_t brigthness = 0;
volatile uint32_t timerCounter = 0;
/*---------------------------------------------------------------------------*/
void init_hardware();
void handle_radio_data();
void send_alive_message();
/*---------------------------------------------------------------------------*/
volatile uint8_t ditheron = 1;
volatile uint8_t ditherCounter = 0;
/*---------------------------------------------------------------------------*/
ISR(TIM0_OVF_vect)
{
    if(ditheron)
    {        
        uint16_t output = brigthness >> 4;
        uint8_t leftover = brigthness & 0x0F;        

        if(ditherCounter < leftover)
        {
            if(output != 0xFF)
                output += 1;
        }        

        if (ditherCounter == 0x00)
        {
            ditherCounter = 0x0F;
        }
        else
        {
            ditherCounter--;
        }        
        
        OCR0A = output;
    }
    else
    {
        OCR0A = brigthness >> 4;
    }
}
/*---------------------------------------------------------------------------*/
ISR(TIM1_OVF_vect)
{
    /* This interrupt ~2 times a second. Exact period is: 0.524288 sec. */
    timerCounter++;        
}
/*---------------------------------------------------------------------------*/
int main()
{ 
    init_hardware();    

    while(1)
    {        
        led_effect();

        if(timerCounter > 100)
        {
            cli(); 
            timerCounter = 0; 
            sei();
            
            send_alive_message();
        }

        handle_radio_data(); 
    }

    return 0;
}
/*---------------------------------------------------------------------------*/
void init_hardware()
{
    /* init hardware pins */
    nrf24_init();
    
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

    /* init pwm at B2 */
    DDRB |= (1 << 2); 
    sbi(TCCR0A,COM0A1);
    sbi(TCCR0A,WGM00);
    sbi(TCCR0A,WGM01);
    sbi(TCCR0B,CS00);
    
    /* init timer1 with fosc/64 prescaler */
    sbi(TCCR1B,CS11);
    sbi(TCCR1B,CS10);

    /* enable timer1 overflow interrupt */
    sbi(TIMSK1,TOIE1);

    /* enable timer0 overflow interrupt */
    sbi(TIMSK0,TOIE0);

    sei();
}
/*---------------------------------------------------------------------------*/
void handle_radio_data()
{    
    if(nrf24_dataReady())
    {    
        nrf24_getData(data_array);  
        /*-------------------------------------------------------------------*/
        /* TODO: Add more commands here ... */      
        /*-------------------------------------------------------------------*/
        switch(data_array[0])      
        {        
            /*---------------------------------------------------------------*/
            /* jump bootloader */
            /*---------------------------------------------------------------*/
            case 0: 
            {
                if(data_array[1] == 0xAA)
                {
                    /* prepare */
                    cli();
                    DDRA = 0x00;
                    DDRB = 0x00;
                    PORTA = 0x00;
                    PORTB = 0x00;                    
                    TCCR0A = 0x00;
                    TCCR1B = 0x00;
                    TIMSK1 = 0x00;

                    /* jump to bootloader! */
                    /* bootloader is located at: 0x1800 */
                    asm volatile("lds r31, 0x18"); // R31 = ZH
                    asm volatile("lds r30, 0x00"); // R30 = ZL
                    asm volatile("icall"); // Jump to the Z register value
                }
                break;
            }
        }            
    }        
}
/*---------------------------------------------------------------------------*/
void send_alive_message()
{
    /* Indicate the type */
    data_array[0] = 0x00;

    /* First put our own RX address values */
    data_array[1] = rx_address[0];
    data_array[2] = rx_address[1];
    data_array[3] = rx_address[2];
    data_array[4] = rx_address[3];
    data_array[5] = rx_address[4];

    /* send and wait ... */
    nrf24_send(data_array);
    while(nrf24_isSending());

    /* make sure you leave enough time after switching back to RX mdoe */
    nrf24_powerUpRx();
    _delay_ms(10);
}
/*---------------------------------------------------------------------------*/
void led_effect()
{
    cli();    
    if(direction)                    
    {
        brigthness += 1;
        ditherCounter = 0;
    }
    else
    {
        brigthness -= 1;
        ditherCounter = 0;
    }
    sei();

    if(brigthness <= 1)
    {
        direction = 1;
        #if 0
        if(ditheron)
            ditheron = 0;
        else
            ditheron = 1;
        #endif
        _delay_ms(100);
    }

    if(brigthness >= 50)
    {
        direction = 0;
    }   

    _delay_ms(50);
}
/*---------------------------------------------------------------------------*/