/*
 ============================================================================
 Name        : UartBBB.c
 Author      : Chad Kidd
 Version     :
 Copyright   : Esterline 2016
 Description : Simple Uart using UART4 on the beaglebone black
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>   // using the termios.h library

#define RX_BUF_SZ	8
#define TX_BUF_SZ	8
/**
 * @brief main entry point for the application
 * @return
 */
int main(void)
{
    struct termios options;               //The termios structure is vital
    int file, count, rx_index = 0;
    unsigned char receive[RX_BUF_SZ];      //declare a buffer for receiving data
    unsigned char transmit[TX_BUF_SZ] = "Hello BeagleBone!";		//buffer for transmit
    char byte_read;

    puts("Opening up the UART");
    //open and initialize UART-4, labeled ttyO4
    if ((file = open("/dev/ttyO4", O_RDWR | O_NOCTTY | O_NDELAY))<0)
    {
	    perror("UART: Failed to open the file.\n");
	    return EXIT_FAILURE;
    }

    tcgetattr(file, &options);            //Sets the parameters associated with file

    // Set up the communications options:
    // 9600 baud, 8-bit, enable receiver, no modem control lines
    options.c_cflag = B9600 | CS8 | CREAD | CLOCAL;
    options.c_iflag = IGNPAR | ICRNL;    //ignore parity errors, CR -> newline
    tcflush(file, TCIFLUSH);             //discard file information not transmitted
    tcsetattr(file, TCSANOW, &options);  //changes occur immmediately

    usleep(1000);                  //wait a little bit

    while (1)
    {
	    if ((count = read(file, &byte_read, 1)) == 1)   //read a byte
	    {
	    	receive[rx_index] = byte_read;
		    rx_index++;
		    if(rx_index >= RX_BUF_SZ)	//if received an entire packet
		    {	//process it
		    	rx_index = 0;
		    	memset(&transmit[0], 0, TX_BUF_SZ * sizeof(unsigned char));
		    	if(receive[0] == 0x01)
		    	{
		    		transmit[0] = 0x01;
		    	}
		    	else if(receive[0] == 0x02)
		    	{
		    		transmit[0] = 0x02;
		    	}
		    	if ((count = write(file, &transmit,TX_BUF_SZ))<0)	//send the response
		    	{   //if an error occurred when writing
		    		perror("Failed to write to the output\n");
		    		close(file);
		    		return EXIT_FAILURE;
		    	}
		    	memset(&receive[0], 0, RX_BUF_SZ * sizeof(unsigned char));
		    }
	    }
    }

    close(file);
    return EXIT_SUCCESS;
}
