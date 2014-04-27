/*-----------------------------------------------------------------------------
/ “THE COFFEEWARE LICENSE” (Revision 1):
/ <ihsan@kehribar.me> wrote this file. As long as you retain this notice you
/ can do whatever you want with this stuff. If we meet some day, and you think
/ this stuff is worth it, you can buy me a coffee in return.
/----------------------------------------------------------------------------*/
#include <avr/io.h>
#include <stdint.h>
#include <avr/wdt.h>
#include <avr/boot.h>
#include "../nrf24.h"
#include "./digital.h"
#include <avr/sleep.h>
#include <avr/power.h>
#include <util/delay.h>
#include <avr/eeprom.h>
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
#define BOOTSTART 0x1800
/*---------------------------------------------------------------------------*/
uint8_t pageBuf[SPM_PAGESIZE];
/*---------------------------------------------------------------------------*/
uint8_t data_array[32];
uint8_t loopCounter = 0;
uint8_t tx_address[5] = {0xBE,0xEF,0xBE,0xEF,0x01};
uint8_t rx_address[5] = {0xD7,0xD7,0xD7,0xD7,0xD7};
/*---------------------------------------------------------------------------*/
static void boot_program_page(uint32_t pageOffset, uint8_t *buf)
{
    uint16_t i;
    
    for(i=0;i<SPM_PAGESIZE;i+=2)
    {
        uint16_t w = *buf++;
        w += (*buf++) << 8;

        boot_page_fill(pageOffset+i,w);
    }

    boot_page_write(pageOffset);
    boot_spm_busy_wait();  
}
/*---------------------------------------------------------------------------*/
void send_ack(uint8_t cmd)
{
    data_array[0] = 0xAA;
    data_array[1] = cmd;
    data_array[2] = 0xAA;    
    data_array[3] = loopCounter++;
    
    nrf24_send(data_array);
    while(nrf24_isSending());
    nrf24_powerUpRx();
}
/*---------------------------------------------------------------------------*/
int main()
{
    uint8_t i;
    uint32_t t32;
    uint32_t pageNum;
    
    /* init hardware pins */
    led0_init();
    nrf24_init();

    /* Channel #2 , payload length: 4 */
    nrf24_config(2,32);

    /* Update the eeprom values to keep in-sync with the app code */    
    t32 = 0;
    for (i = 0; i < 5; i++)
    {
        if(rx_address[i] != eeprom_read_byte((uint8_t*)(10+i)))
        {
            t32 = 1; /* means you should update the eeprom ... */
        }
    }     
    if(t32)
    {
        for (i = 0; i < 5; i++)
        {
            eeprom_write_byte((uint8_t*)(10+i),rx_address[i]);
        }        
    }

    /* Set the device addresses */
    nrf24_tx_address(tx_address);
    nrf24_rx_address(rx_address);    

    /* inform the dongle */
    send_ack(0x04);

    nrf24_powerUpRx();

    while(1)
    {                            
        if(nrf24_dataReady())
        {            
            nrf24_getData(data_array);
            switch(data_array[0])
            {
                case 0: /* bootloader entry response */
                {
                    if(data_array[1] == 0xAA)
                    {
                        send_ack(0x00);                        
                    }                      
                    break;
                }
                case 1: /* fill page buffer */
                {
                    uint8_t index = 0;
                    uint8_t len = data_array[2];
                    uint8_t start = data_array[1];                    
                    for(i = start; i < (start+len); i++)
                    {
                        pageBuf[i] = data_array[index+3];
                        index += 1;
                    }
                    send_ack(0x01);                                 
                    break;
                }
                case 2: /* program the buffer */
                {
                    pageNum = data_array[1];

                    t32 = data_array[2];
                    t32 = t32 << 8;
                    pageNum += t32;

                    /*t32 = data_array[3];
                    t32 = t32 << 16;
                    pageNum += t32;

                    t32 = data_array[4];
                    t32 = t32 << 24;
                    pageNum += t32;*/

                    boot_program_page(pageNum,pageBuf);
                    send_ack(0x02);
                    break;
                }
                case 3: /* delete the pages */
                {
                    for(t32=0;t32<BOOTSTART;t32+=SPM_PAGESIZE)
                    {
                        boot_page_erase(t32);
                        boot_spm_busy_wait();               
                    }    
                    send_ack(0x03);                    
                    break;       
                }
                case 4: /* jump to the user app */
                {
                    send_ack(0x04);                    
                    PORTA = 0x00;
                    PORTB = 0x00;
                    DDRA = 0x00;
                    DDRB = 0x00;
                    asm volatile("lds r31, 0x00"); // R31 = ZH
                    asm volatile("lds r30, 0x00"); // R30 = ZL
                    asm volatile("icall"); // Jump to the Z register value 
                    break;               
                }
            }                        
        }                        
    }

    return 0;
}
