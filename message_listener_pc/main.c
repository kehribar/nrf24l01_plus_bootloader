/*-----------------------------------------------------------------------------
/ “THE COFFEEWARE LICENSE” (Revision 1):
/ <ihsan@kehribar.me> wrote this file. As long as you retain this notice you
/ can do whatever you want with this stuff. If we meet some day, and you think
/ this stuff is worth it, you can buy me a coffee in return.
/----------------------------------------------------------------------------*/
#include <netdb.h>
#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
/*---------------------------------------------------------------------------*/
#include "serial_lib.h"
/*---------------------------------------------------------------------------*/
#define VERBOSE 0
#define TIMEOUT 150
/*---------------------------------------------------------------------------*/
uint8_t dataBuffer[65536];
uint8_t tx_addressBuffer[5];
uint8_t rx_addressBuffer[5];
/*---------------------------------------------------------------------------*/
int getTransmissionResult(int serialPort)
{
	dataBuffer[0] = 1; /* msg len */
	dataBuffer[1] = 7; /* command */

	write(serialPort,dataBuffer,2);
	readRawBytes(serialPort,dataBuffer,1,20000);
	
	return (int8_t)dataBuffer[0];
}
/*---------------------------------------------------------------------------*/
int isSending(int serialPort)
{
	dataBuffer[0] = 1; /* msg len */
	dataBuffer[1] = 6; /* command */

	write(serialPort,dataBuffer,2);
	int rc = readRawBytes(serialPort,dataBuffer,1,20000);

	return (int8_t)dataBuffer[0];
}
/*---------------------------------------------------------------------------*/
int getRxFifoCount(int serialPort)
{
	dataBuffer[0] = 1; /* msg len */
	dataBuffer[1] = 9; /* command */

	write(serialPort,dataBuffer,2);
	readRawBytes(serialPort,dataBuffer,2,20000);

	return (uint16_t)(dataBuffer[0]+(dataBuffer[1]<<8));
}
/*---------------------------------------------------------------------------*/
int pullDataFromFifo(int serialPort,int len, uint8_t* rxBuffer)
{
	dataBuffer[0] = 2; /* msg len */
	dataBuffer[1] = 10; /* command */
	dataBuffer[2] = len; /* fifo read length */

	write(serialPort,dataBuffer,3);
	return readRawBytes(serialPort,rxBuffer,len,20000);
}
/*---------------------------------------------------------------------------*/
int getVersion(int serialPort)
{
	dataBuffer[0] = 1; /* msg len */
	dataBuffer[1] = 1; /* command */

	write(serialPort,dataBuffer,2);
	readRawBytes(serialPort,dataBuffer,1,20000);

	return (int8_t)dataBuffer[0];
}
/*---------------------------------------------------------------------------*/
int loopbackTest(int serialPort)
{
	const uint8_t testLength = 32;

	dataBuffer[0] = testLength; /* msg len */
	dataBuffer[1] = 2; /* command */

	for (int i = 0; i < testLength; ++i)
	{
		dataBuffer[i+2] = i;
	}
	
	write(serialPort,dataBuffer,testLength+2);
	readRawBytes(serialPort,dataBuffer,testLength,20000);

	for (int i = 1; i < testLength; ++i)
	{
		if(dataBuffer[i] != i-1)
		{
			printf("Data mismatch: got: %d expected: %d\r\n",dataBuffer[i],i-1);
			return -1;
		}
	}

	return 0;
}
/*---------------------------------------------------------------------------*/
int sendRadioMessage(int serialPort, uint8_t* buf, int len)
{
	int rc;
	int timeoutCounter;
	uint8_t rxBuff[32];

	write(serialPort,buf,len);
	readRawBytes(serialPort,dataBuffer,1,20000);

	if(dataBuffer[0] != 0xAA)
	{
		printf("[sendRadioMessage]: ACK error!\n");
		return -1;
	}

	timeoutCounter = 0;

	while(isSending(serialPort) && (timeoutCounter < TIMEOUT))
	{
		timeoutCounter++;
	}
	
	if(!(timeoutCounter < TIMEOUT))
	{
		// printf("[sendRadioMessage]: Timeout err!\n");
		return -1;
	}
	
	if(getTransmissionResult(serialPort) == 0)
	{
		timeoutCounter = 0;

		while((getRxFifoCount(serialPort) == 0) && (timeoutCounter < TIMEOUT))
		{
			timeoutCounter++;
		}    

		if(!(timeoutCounter < TIMEOUT))
		{
			// printf("[sendRadioMessage]: Timeout err!\n");
			return -1;
		}

		pullDataFromFifo(serialPort,32,rxBuff);
		return 0;
	}
	else
	{
		// printf("[sendRadioMessage]: Missing message?\n");
		return -1;
	}
}
int setTXAddress(int serialPort, uint8_t* buf)
{
	dataBuffer[0] = 6; /* msg len */
	dataBuffer[1] = 4; /* uC command */
	
	dataBuffer[2] = buf[0];
	dataBuffer[3] = buf[1]; 
	dataBuffer[4] = buf[2];
	dataBuffer[5] = buf[3];
	dataBuffer[6] = buf[4];

	write(serialPort,dataBuffer,7);
	readRawBytes(serialPort,dataBuffer,1,20000);

	if(dataBuffer[0] != 0xAA)
	{
		printf("[sendRadioMessage]: ACK error!\n");
		return -1;
	}
	
	return 0;
}
/*---------------------------------------------------------------------------*/
int setRXAddress(int serialPort, uint8_t* buf)
{
	dataBuffer[0] = 6; /* msg len */
	dataBuffer[1] = 3; /* uC command */
	
	dataBuffer[2] = buf[0];
	dataBuffer[3] = buf[1]; 
	dataBuffer[4] = buf[2];
	dataBuffer[5] = buf[3];
	dataBuffer[6] = buf[4];

	write(serialPort,dataBuffer,7);
	readRawBytes(serialPort,dataBuffer,1,20000);

	if(dataBuffer[0] != 0xAA)
	{
		printf("[sendRadioMessage]: ACK error!\n");
		return -1;
	}
	
	return 0;
}
/*---------------------------------------------------------------------------*/
/* taken from: http://stackoverflow.com/a/7776146 */
void hexDump (char *desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Output description if given.
    if (desc != NULL)
        printf ("%s:\n", desc);

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.
            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf ("  %s\n", buff);
}

/*---------------------------------------------------------------------------*/
int main(int argc, char**argv)
{
	int i;
	int fd = 0;
	uint16_t trial = 0;
	char* portPath = NULL;
	char* rx_addressString = NULL;
	char* tx_addressString = NULL;

	printf("> Simple message listener for tea-bootloader compatible nodes\n");        
	
	/*-----------------------------------------------------------------------*/
	
	if(argc == 4)
	{
		portPath = argv[1];		
		rx_addressString = argv[2];
		sscanf(rx_addressString,"%X:%X:%X:%X:%X",rx_addressBuffer,rx_addressBuffer+1,rx_addressBuffer+2,rx_addressBuffer+3,rx_addressBuffer+4);
		tx_addressString = argv[3];
		sscanf(tx_addressString,"%X:%X:%X:%X:%X",tx_addressBuffer,tx_addressBuffer+1,tx_addressBuffer+2,tx_addressBuffer+3,tx_addressBuffer+4);
	}
	else
	{
		printf("> Argument parsing error!\n");
		printf("> Usage: [port path] [dongle RX address] [node RX address]\n");
		printf("> Example: ./main /dev/tty.usbserial-A900cbrd 0xE7:0xE7:0xE7:0xE7:0x00 0xD7:0xD7:0xD7:0xD7:0xD7\n");
		return 0;
	}			

	/*-----------------------------------------------------------------------*/	   

    fd = serialport_init(portPath,115200,'n');

    if(fd < 0)
    {
        printf("[err]: Connection error.\n");
        return 0;
    }
    else
   	{
        printf("> Conection OK.\n");
        serialport_flush(fd);
        printf("> Serial port flush OK.\n");   

        if(loopbackTest(fd) != 0)
        {
        	printf("> Loopback test failed!\r\n");
        	return 0;
        }
        else
        {        	
        	uint8_t version = getVersion(fd);
        	printf("> Dongle version: %d.%d\r\n",(version>>4),(version&0x0F));
        }                    
    }   

    printf("> Setting the TX address ...\n");
    setTXAddress(fd,tx_addressBuffer);

    printf("> Setting the RX address ...\n");
    setRXAddress(fd,rx_addressBuffer);

    while(1)
    {
    	uint8_t thisBuffer[1024];
    	char sendBuffer[256];
    	uint8_t len;
    	int temp;
    	float realval;
    	if(getRxFifoCount(fd))
    	{
			len = getRxFifoCount(fd);
			printf("> ---------------------------------------------------------------------------\n");
			printf("> New message!\n");
			printf("> Length: %d\n",len);
			pullDataFromFifo(fd,len,thisBuffer);

			hexDump("> Dump",thisBuffer,len);			

			temp = (thisBuffer[0]<<8)+thisBuffer[1];
			realval  = ((float)temp * 1100.0) / 1024.0;              
			realval -= 500;
			realval /= 10.0;

			printf("> Raw: %d\n",temp);
			printf("> Readout: %f\n",realval);
			sprintf(sendBuffer,"./phant_client.rb %f",realval);
			system(sendBuffer);
			printf("> Phant.io send process done.\n");
    	}
    }


}
/*---------------------------------------------------------------------------*/