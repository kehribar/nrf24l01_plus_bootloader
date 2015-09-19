/*-----------------------------------------------------------------------------
/
/
/
/
/----------------------------------------------------------------------------*/

// Port for the I2C
#define I2C_DDR DDRA
#define I2C_PIN PINA
#define I2C_PORT PORTA

// Pins to be used in the bit banging
#define I2C_CLK 0
#define I2C_DAT 1

#define I2C_DELAY() _delay_us(10)

#define I2C_DATA_HI() I2C_DDR &= ~( 1 << I2C_DAT );
#define I2C_DATA_LO() I2C_DDR |= ( 1 << I2C_DAT );

#define I2C_CLOCK_HI() I2C_DDR &= ~( 1 << I2C_CLK );
#define I2C_CLOCK_LO() I2C_DDR |= ( 1 << I2C_CLK );

// ----------------------------------------------------------------------------
#include "./bitbang_i2c.h"

void I2C_WriteBit( unsigned char c )
{
	if ( c > 0 )
	{
		I2C_DATA_HI();
	}
	else
	{
		I2C_DATA_LO();
	}

	I2C_CLOCK_HI();
	I2C_DELAY();
	
	I2C_CLOCK_LO();
	I2C_DELAY();

	if ( c > 0 )
	{
		I2C_DATA_LO();
	}
}

unsigned char I2C_ReadBit()
{
	I2C_DATA_HI();

	I2C_CLOCK_HI();
	I2C_DELAY();

	unsigned char c = I2C_PIN;

	I2C_CLOCK_LO();
	I2C_DELAY();

	return ( c >> I2C_DAT ) & 1;
}

void I2C_Init()
{
	I2C_PORT &= ~( ( 1 << I2C_DAT ) | ( 1 << I2C_CLK ) );

	I2C_CLOCK_HI();
	I2C_DATA_HI();

	I2C_DELAY();
}

void I2C_Start()
{
	// set both to high at the same time
	I2C_DDR &= ~( ( 1 << I2C_DAT ) | ( 1 << I2C_CLK ) );
	I2C_DELAY();

	I2C_DATA_LO();
	I2C_DELAY();

	I2C_CLOCK_LO();
	I2C_DELAY();
}

void I2C_Stop()
{
	I2C_DATA_LO();
	I2C_CLOCK_LO();
	I2C_DELAY();

	I2C_CLOCK_HI();
	I2C_DELAY();

	I2C_DATA_HI();
	I2C_DELAY();
}

unsigned char I2C_Write( unsigned char c )
{
	char i;

	for (i=0;i<8;i++)
	{
		I2C_WriteBit( c & 128 );

		c<<=1;
	}

	return I2C_ReadBit();
}

unsigned char I2C_Read( unsigned char ack )
{
	unsigned char res = 0;
	char i;

	for (i=0;i<8;i++)
	{
		res <<= 1;
		res |= I2C_ReadBit();
	}

	if ( ack > 0)
	{
		I2C_WriteBit( 0 );
	}
	else
	{
		I2C_WriteBit( 1 );
	}

	I2C_DELAY();

	return res;
}
// ----------------------------------------------------------------------------
