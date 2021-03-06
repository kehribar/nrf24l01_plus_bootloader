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
#define VERSION 0x10
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
ISR(USART_RX_vect) 
{
    uint8_t tmp = UDR0;     
    
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
void uart_init()
{
    #if 0
        /* 38400 baud rate with 16 MHz Xtal ... */
        const uint8_t ubrr = 51; 
    #else
        /* 115200 baud rate with 16 MHz Xtal ... */
        const uint8_t ubrr = 16; 
    #endif
    
    UCSR0A = (1<<U2X0);

    DDRD &= ~(1<<0);
    DDRD |=  (1<<1);

    /* Set baud rate */ 
    UBRR0H = (unsigned char)(ubrr>>8); 
    UBRR0L = (unsigned char)ubrr; 

    /* Enable receiver and transmitter and Receive interupt */ 
    UCSR0B = (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0);

    /* Set frame format: 8data, 1stop bit no parity */ 
    UCSR0C = (1<<UCSZ01)|(1<<UCSZ00); 

}
/* ------------------------------------------------------------------------- */
void uart_put_char(uint8_t tx)
{
    /* Wait for empty transmit buffer ... */
    while(!(UCSR0A & (1<<UDRE0)));

    /* Start sending the data! */
    UDR0 = tx;

    /* Wait until the transmission is over ... */
    while(!(UCSR0A & (1<<TXC0)));
}
/* ------------------------------------------------------------------------- */
uint8_t temp;
uint8_t q = 0;
uint8_t data_array[32];
uint8_t tx_address[5] = {0xD7,0xD7,0xD7,0xD7,0xD7};
uint8_t rx_address[5] = {0xBE,0xEF,0xBE,0xEF,0x01};
/* ------------------------------------------------------------------------- */
void handle_newMessage()
{
    uint8_t i;        

    switch(rxBuffer[0])
    {
        case 0: /* blink an LED */
        {
            digitalWrite(C,0,HIGH);
            _delay_ms(20);
            digitalWrite(C,0,LOW);
            _delay_ms(20);
            digitalWrite(C,0,HIGH);
            _delay_ms(20);
            digitalWrite(C,0,LOW);
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
    uint32_t loopCounter = 0;

    /* init the radio buffer */
    RingBuffer_InitBuffer(&Buffer, BufferData, sizeof(BufferData));
 
    /* init debug led */
    pinMode(C,0,OUTPUT);

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

        if(loopCounter++ > 1000)
        {
            loopCounter = 0;
            togglePin(C,0);
        }        
    }
}
/* ------------------------------------------------------------------------- */