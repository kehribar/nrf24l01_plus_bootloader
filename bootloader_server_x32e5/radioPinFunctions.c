/*
* ----------------------------------------------------------------------------
* “THE COFFEEWARE LICENSE” (Revision 1):
* <ihsan@kehribar.me> wrote this file. As long as you retain this notice you
* can do whatever you want with this stuff. If we meet some day, and you think
* this stuff is worth it, you can buy me a coffee in return.
* -----------------------------------------------------------------------------
* Please define your platform spesific functions in this file ...
* -----------------------------------------------------------------------------
*/

#include <avr/io.h>
#include "digital.h"

/* ------------------------------------------------------------------------- */
void nrf24_setupPins()
{
    // CE output
    pinMode(A,0,OUTPUT);
    
    // CSN output
    pinMode(A,2,OUTPUT);

    // SCK output
    pinMode(C,5,OUTPUT);

    // MOSI output
    pinMode(C,7,OUTPUT);

    // MISO input
    pinMode(C,6,INPUT);
}
/* ------------------------------------------------------------------------- */
void nrf24_ce_digitalWrite(uint8_t state)
{
    if(state)
    {
        digitalWrite(A,0,HIGH);
    }
    else
    {
        digitalWrite(A,0,LOW);
    }
}
/* ------------------------------------------------------------------------- */
void nrf24_csn_digitalWrite(uint8_t state)
{
    if(state)
    {
        digitalWrite(A,2,HIGH);
    }
    else
    {
        digitalWrite(A,2,LOW);
    }
}
/* ------------------------------------------------------------------------- */
void nrf24_sck_digitalWrite(uint8_t state)
{
    if(state)
    {
        digitalWrite(C,5,HIGH);
    }
    else
    {
        digitalWrite(C,5,LOW);
    }
}
/* ------------------------------------------------------------------------- */
void nrf24_mosi_digitalWrite(uint8_t state)
{
    if(state)
    {
        digitalWrite(C,7,HIGH);
    }
    else
    {
        digitalWrite(C,7,LOW);
    }
}
/* ------------------------------------------------------------------------- */
uint8_t nrf24_miso_digitalRead()
{
    return digitalRead(C,6);
}
/* ------------------------------------------------------------------------- */
