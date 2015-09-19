/*
* ----------------------------------------------------------------------------
* “THE COFFEEWARE LICENSE” (Revision 1):
* <ihsan@kehribar.me> wrote this file. As long as you retain this notice you
* can do whatever you want with this stuff. If we meet some day, and you think
* this stuff is worth it, you can buy me a coffee in return.
* -----------------------------------------------------------------------------
*/

#include <avr/io.h>
#include <stdint.h>
#include "../nrf24.h"
#include "./digital.h"
#include <util/delay.h>
#include "./ringBuffer.h"
#include "./util/xprintf.h"
#include <avr/interrupt.h> 
/* ------------------------------------------------------------------------- */
#define DONE 0xAA
#define VERSION 0x11
uint8_t listeningMode = 1;
uint8_t lastMessageStatus;
/* ------------------------------------------------------------------------- */
#define getHighByte(word) (word>>8)
#define getLowByte(word) (word&0xFF)
/* ------------------------------------------------------------------------- */
RingBuffer_t Buffer; /* ring buffer for incoming radio messages */
uint8_t      BufferData[1024];
/* ------------------------------------------------------------------------- */
volatile uint8_t msgLen;
volatile uint8_t bufCount;
volatile uint8_t newMessage;
volatile uint8_t rxBuffer[64];
volatile uint8_t waitingForNewMessage = 1;
/* ------------------------------------------------------------------------- */
ISR(USARTD0_RXC_vect)
{ 
    uint8_t tmp = USARTD0.DATA;     
    
    if(waitingForNewMessage)
    {         
        bufCount = 0;
        msgLen = tmp;
        waitingForNewMessage = 0;
    }
    else if(bufCount < msgLen)
    {
        rxBuffer[bufCount++] = tmp;
        if(bufCount == msgLen)
        {
            newMessage = 1;
        }
    }
}
/* ------------------------------------------------------------------------- */
void initClock_32Mhz()
{
    /* Generates 32Mhz clock from internal 2Mhz clock via PLL */
    OSC.PLLCTRL = OSC_PLLSRC_RC2M_gc | 16;
    OSC.CTRL |= OSC_PLLEN_bm ;
    while((OSC.STATUS & OSC_PLLRDY_bm) == 0);
    CCP = CCP_IOREG_gc;
    CLK.CTRL = CLK_SCLKSEL_PLL_gc;
}
/* ------------------------------------------------------------------------- */
void uart_init()
{
    /* Set UART pin driver states */
    pinMode(D,6,INPUT);
    pinMode(D,7,OUTPUT);

    /* Remap the UART pins */
    PORTD.REMAP = PORT_USART0_bm;

    USARTD0.CTRLB = USART_RXEN_bm|USART_TXEN_bm;
    USARTD0.CTRLC = USART_CMODE_ASYNCHRONOUS_gc|USART_PMODE_DISABLED_gc|USART_CHSIZE_8BIT_gc;

    /* 115200 baud rate with 32MHz clock */
    USARTD0.BAUDCTRLA = 131; USARTD0.BAUDCTRLB = (-3 << USART_BSCALE_gp); 

    /* Enable UART data reception interrupt */
    USARTD0.CTRLA |= USART_DRIE_bm;
    
    /* Set UART data reception interrupt priority */
    USARTD0.CTRLA |= (1<<5);
    USARTD0.CTRLA |= (1<<4);
  
    /* Enable all interrupt levels */
    PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
    sei();
}
/* ------------------------------------------------------------------------- */
void uart_put_char(uint8_t tx)
{
    while(!(USARTD0.STATUS & USART_DREIF_bm));
    USARTD0.DATA = tx;
}
/* ------------------------------------------------------------------------- */
uint8_t temp;
uint8_t q = 0;
uint8_t data_array[32];
uint8_t tx_address[5] = {0xD7,0xD7,0xD7,0xD7,0x00};
uint8_t rx_address[5] = {0xE7,0xE7,0xE7,0xE7,0x00};
/* ------------------------------------------------------------------------- */
void handle_newMessage()
{
    uint8_t i;        

    switch(rxBuffer[0])
    {
        case 0: /* blink an LED */
        {

            break;
        }
        case 1: /* read version */
        {
            uart_put_char(VERSION);
            break;
        }
        case 2: /* loopback */
        {            
            for(i=0;i<msgLen;i++)
            {
                uart_put_char(rxBuffer[i]);
            }
            break;
        }
        case 3: /* set rx address */
        {
            nrf24_rx_address((uint8_t*)(rxBuffer+1));
            uart_put_char(DONE);
            break;
        }
        case 4: /* set tx address */
        {       
            nrf24_tx_address((uint8_t*)(rxBuffer+1));
            uart_put_char(DONE);
            break;
        }
        case 5: /* send message */
        {                   
            nrf24_send((uint8_t*)(rxBuffer+1));                                                            
            uart_put_char(DONE);
            break;
        }
        case 6: /* get tx sending status */
        {
            uart_put_char(nrf24_isSending());            
            break;
        }
        case 7: /* get last transmission result */
        {
            lastMessageStatus = nrf24_lastMessageStatus();            
            if(listeningMode)
            {
                nrf24_powerUpRx();
            }
            uart_put_char(lastMessageStatus);
            break;
        }
        case 8: /* set listening mode */
        {
            listeningMode = rxBuffer[1];
            if(listeningMode)
            {
                nrf24_powerUpRx();
            }
            else
            {
                nrf24_powerDown();
            }
            break;
        }
        case 9: /* get rx buffer fifo count */
        {
            uint16_t fifoSize;

            fifoSize = RingBuffer_GetCount(&Buffer);

            uart_put_char(getLowByte(fifoSize));
            uart_put_char(getHighByte(fifoSize));

            break;
        }
        case 10: /* pull data from RX fifo */
        {
            for(i=0;i<rxBuffer[1];i++)
            {                
                if(RingBuffer_IsEmpty(&Buffer))
                {
                    uart_put_char(0x00);
                }
                else
                {
                    uart_put_char(RingBuffer_Remove(&Buffer));
                }                
            }

            break;
        }
        default:
        {          
            break;            
        }
    }    

    for (i = 0; i < sizeof(rxBuffer); ++i)
    {
        rxBuffer[i] = 0xFF;
    }

    cli();
    newMessage = 0;
    waitingForNewMessage = 1;
    sei();
}
/* ------------------------------------------------------------------------- */
void handle_nrf24_data()
{    
    uint8_t i = 0;
    nrf24_getData(data_array);    
    while((!RingBuffer_IsFull(&Buffer)) && (i<32))
    {
        RingBuffer_Insert(&Buffer,data_array[i++]);
    }
}
/* ------------------------------------------------------------------------- */
int main()
{
    initClock_32Mhz();

    /* init the radio buffer */
    RingBuffer_InitBuffer(&Buffer, BufferData, sizeof(BufferData));
 
    /* init the software uart */
    uart_init();

    /* init hardware pins */
    nrf24_init();
    
    /* Channel #2 , payload length: 4 */
    nrf24_config(2,32);
 
    /* Set default device addresses */
    nrf24_tx_address(tx_address);
    nrf24_rx_address(rx_address);

    sei();

    nrf24_powerUpRx();

    while(1)
    {    
        if(newMessage)
        {
            handle_newMessage();
        }

        if(nrf24_dataReady())
        {
            handle_nrf24_data();
        }      
    }
}
/* ------------------------------------------------------------------------- */