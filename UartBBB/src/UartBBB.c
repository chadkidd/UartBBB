/*
 ============================================================================
 Name        : UartBBB.c
 Author      : Chad Kidd
 Version     :
 Copyright   : Esterline 2016
 Description : Simple Uart using UART4 on the beaglebone black
 ============================================================================
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>   // using the termios.h library
#include <arpa/inet.h>
#include <linux/types.h>

#define APP_VER	    	"01.00"
#define IP_PING			"192.168.0.101"

#define UART_CMD_VER	0x01
#define UART_CMD_PING	0x0B
#define UART_RX_ACK		0xFB
#define UART_RX_INVALID	0xFE

#define TX_BUF_SZ		16

static int ping(char *ipaddr);

/**
 *
 * @brief main entry point for the application
 * @return
 */
int main(void)
{
    struct termios options;               //The termios structure is vital
    int status, file, count;
    char transmit[TX_BUF_SZ];		//buffer for transmit
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

    while (1)
    {
	    if ((count = read(file, &byte_read, 1)) == 1)   //read a byte
	    {
			memset(&transmit[0], 0, TX_BUF_SZ * sizeof(unsigned char));

			if(byte_read == UART_CMD_VER)
			{
				transmit[0] = UART_RX_ACK;
				count = write(file, &transmit[0], 1);	//send ack
				puts("Firmware version requested");
				transmit[0] = UART_CMD_VER;
				strncpy(&transmit[1], APP_VER, sizeof(APP_VER)-1);
				count = write(file, &transmit[0], sizeof(APP_VER));	//send response
			}
			else if(byte_read == UART_CMD_PING)
			{
				transmit[0] = UART_RX_ACK;
				count = write(file, &transmit[0], 1);	//send ack
				puts("Pinging IP address");
				status = ping(IP_PING);
				transmit[0] = UART_CMD_PING;
				if(status == EXIT_SUCCESS)
					strncpy(&transmit[1], IP_PING, sizeof(IP_PING)-1);
				else
					strncpy(&transmit[1], "No connection", sizeof(IP_PING));
				count = write(file, &transmit[0], sizeof(IP_PING));	//send response
			}
			else	//invalid command received
			{
				transmit[0] = UART_RX_INVALID;
				count = write(file, &transmit[0], 1);	//send ack
			}
			if (count<0)	//if write failed
			{   //if an error occurred when writing
				perror("Failed to write to the output\n");
				close(file);
				return EXIT_FAILURE;
			}

	    }
    }

    close(file);
    return EXIT_SUCCESS;
}


static int ping(char *ipaddr)
{
	int i;
	char *command = NULL;
	char *s = malloc(sizeof(char) * 200);
	FILE *fp;

	int stat = 0;
	asprintf (&command, "%s %s -c 1", "ping", ipaddr);		//ping, count = 1
	fp = popen(command, "r");
	if(fp == NULL)
	{
		perror("Failed to execute ping command");
		free(command);
		free(s);
		return EXIT_FAILURE;
	}

	for(i = 0; i < 4; i++)
	{
		usleep(10000);		//delay a few milliseconds for ping to work
		fgets(s, sizeof(char)*200, fp);
		printf("%s", s);
		if(strstr(s, "icmp_seq=1") != 0)
			break;
	}

	stat = pclose(fp);
	free(command);
	free(s);
	return WEXITSTATUS(stat);		//return the status of the ping command

}
