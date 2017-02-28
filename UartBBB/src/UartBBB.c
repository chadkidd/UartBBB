/*
 ============================================================================
 Name        : UartBBB.c
 Author      : Chad Kidd
 Version     :
 Copyright   :
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

#define APP_VER	    	"01.01"
#define IP_ADDRS		"192.168.0.101"

#define UART_CMD_VER	0x01
#define UART_CMD_SHDN	0x02
#define UART_CMD_READ	0x03		//allows multiple reads of tux.bmp file to be performed
									//this is a 3-byte command, all others are single byte commands
#define UART_CMD_PING	0x0B
#define UART_RX_ACK		0xFB
#define UART_RX_INVALID	0xFE

#define RX_BUF_SZ		3
#define TX_BUF_SZ		16

static int ping(char *ipaddr);
static int minicom_start_stop();
static void system_shutdown();
static int read_file(char *ipaddr);

/**
 *
 * @brief main entry point for the application
 * @return
 */
int main(void)
{
    struct termios options;               //The termios structure is vital
    int status, file, count, read_count, read_index, rx_index = 0;
    char transmit[TX_BUF_SZ];		//buffer for transmit
    char receive[RX_BUF_SZ];		//buffer for receive
    char byte_read;

    minicom_start_stop();

    puts("Opening up the UART");
    //open and initialize UART-4, labeled ttyO4
    //open in nonblocking mode so we won't block waiting for carrier detect.
    if ((file = open("/dev/ttyO4", O_RDWR | O_NOCTTY | O_NONBLOCK))<0)
    {
	    perror("UART: Failed to open the file.\n");
	    return EXIT_FAILURE;
    }

    //now that serial port is open, we can turn off non-blocking behavior
    //fcntl(file, F_SETFL, fcntl(file, F_GETFL) & ~O_NONBLOCK);

    tcgetattr(file, &options);            //Sets the parameters associated with file

    // Set up the communications options:
    // 9600 baud, 8-bit, enable receiver, no modem control lines
    options.c_cflag = B9600 | CS8 | CREAD | CLOCAL;
    options.c_iflag = IGNPAR | ICRNL;    //ignore parity errors, CR -> newline
    tcflush(file, TCIFLUSH);             //discard file information not transmitted
    tcsetattr(file, TCSANOW, &options);  //changes occur immmediately

    while (1)
    {
    	usleep(15);		//delay a little to keep from hogging resources
	    if ((count = read(file, &byte_read, 1)) == 1)   //read a byte
	    {
			receive[rx_index] = byte_read;
			rx_index++;
			if(receive[0] != UART_CMD_READ || rx_index >= RX_BUF_SZ)
			{	//if this is a single byte command or the last byte of UART_CMD_READ
				rx_index = 0;
				memset(&transmit[0], 0, TX_BUF_SZ * sizeof(unsigned char));
				switch (receive[0])
				{
					case UART_CMD_VER:
						transmit[0] = UART_RX_ACK;
						count = write(file, &transmit[0], 1);	//send ack
						puts("Firmware version requested");
						transmit[0] = UART_CMD_VER;
						strncpy(&transmit[1], APP_VER, sizeof(APP_VER)-1);
						count = write(file, &transmit[0], sizeof(APP_VER));	//send response
						break;

					case UART_CMD_SHDN:
						transmit[0] = UART_RX_ACK;
						count = write(file, &transmit[0], 1);	//send ack
						system_shutdown();
						break;

					case UART_CMD_READ:
						transmit[0] = UART_RX_ACK;
						count = write(file, &transmit[0], 1);	//send ack
						read_count = receive[1] << 8;	//store MSB of read count
						read_count |= receive[2] & 0xFF;	//store LSB of read count
						for(read_index = 1; read_index <= read_count; read_index ++)
						{	//read the file and send up a response for every attempt
							status = read_file(IP_ADDRS);
							transmit[0] = UART_CMD_READ;
							transmit[1] = status & 0xFF;
							transmit[2] = (read_index & 0xFF00) >> 8;		//send MSB of read_index
							transmit[3] = read_index & 0xFF;			//send LSB of read_index
							count = write(file, &transmit[0], 4);	//send response
						}
						break;

					case UART_CMD_PING:
						transmit[0] = UART_RX_ACK;
						count = write(file, &transmit[0], 1);	//send ack
						puts("Pinging IP address");
						status = ping(IP_ADDRS);
						transmit[0] = UART_CMD_PING;
						if(status == EXIT_SUCCESS)
						{	//now, attempt to do an scp file read
							status = read_file(IP_ADDRS);
							if(status == EXIT_SUCCESS)
								strncpy(&transmit[1], IP_ADDRS, sizeof(IP_ADDRS)-1);
							else
								strncpy(&transmit[1], "Read failure!", sizeof(IP_ADDRS));
						}
						else
						{
							strncpy(&transmit[1], "Ping failure!", sizeof(IP_ADDRS));
						}
						count = write(file, &transmit[0], sizeof(IP_ADDRS));	//send response
						break;

					default:
						transmit[0] = UART_RX_INVALID;
						count = write(file, &transmit[0], 1);	//send ack
						break;
				}
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

/**
 * @brief Starts and stops minicom to get the UART working after system startup
 * @param none
 * @return 0 if success, else failure code
 */
static int minicom_start_stop()
{
	int i;
	char *command = NULL;
	char *s = malloc(sizeof(char) * 200);
	FILE *fp;

	int stat = 0;
	asprintf (&command, "minicom -b 9600 -o -D /dev/ttyO4");		//launch minicom
	fp = popen(command, "r");
	if(fp == NULL)
	{
		perror("Failed to execute minicom command");
		free(command);
		free(s);
		return EXIT_FAILURE;
	}

	for(i = 0; i < 10; i++)
	{
		usleep(1000);		//delay a little
		fgets(s, sizeof(char)*200, fp);
		printf("%s", s);
		if(strstr(s, "Press CTRL-A") != 0)
			break;
	}

	usleep(2000);		//delay a little
	free(command);

	//kill minicom
	asprintf (&command, "killall -9 minicom");		//kill minicom
	fp = popen(command, "r");
	for(i = 0; i < 4; i++)
	{
		usleep(1000);		//delay a little
		fgets(s, sizeof(char)*200, fp);
		printf("%s", s);
		if(strstr(s, "Killed") != 0)
			break;
	}
	stat = pclose(fp);

	free(command);
	free(s);
	usleep(2000);		//delay a little
	return WEXITSTATUS(stat);		//return the status
}

/**
 * @brief pings an IP address using the ping command
 * @param ipaddress to ping
 * @return 0 if success, else failure code
 */
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

/**
 * @brief reads a file from an attached ethernet device using scp
 * @param ipaddress to read
 * @return 0 if success, else failure code
 */
static int read_file(char *ipaddr)
{
	int i;
	char *command = NULL;
	char *s = malloc(sizeof(char) * 200);
	FILE *fp;

	int stat = 0;
	asprintf (&command, "sudo /../usr/bin/./copyBmp");		//start the copybmp script
	fp = popen(command, "r");
	if(fp == NULL)
	{
		perror("Failed to start the copyBmp script");
		free(command);
		free(s);
		return EXIT_FAILURE;
	}

	for(i = 0; i < 3; i++)
	{
		usleep(10000);		//delay a bit for response to be received
		fgets(s, sizeof(char)*200, fp);
		printf("%s", s);
		if((strstr(s, "copy success") != 0) || (strstr(s, "copy failure") != 0))
			break;
	}

	stat = pclose(fp);
	free(command);
	free(s);
	return WEXITSTATUS(stat);		//return the status of the ping command

}

/**
 * @brief Shuts down the system
 * @param none
 * @return none
 */
static void system_shutdown()
{
	char *command = NULL;
	FILE *fp;

	asprintf (&command, "sudo systemctl poweroff");		//shut down the system
	fp = popen(command, "r");
	if(fp == NULL)
		perror("Failed to execute shutdown command");

	free(command);

}
