/*
 * File:			main.c
 * 
 * Author:			Zoltan Gere
 * Created:			05/16/20
 * Description:		SensorMaster
 * 					main function, the entry point
 * 					Read README.md for further information
 * 
 * <MIT License>
 */

#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <stdio.h>
#include <stdlib.h>

/* Command line arguments:
 *		-c -l <master's_logfile> mfile <filename> sensortype <NTC|SCC30> sensoraddress <address> echo {0|1} interval <t>
 *		-f -l <master's_logfile> inputfile_containing_command
 */
int main(int argc, char *argv[])
{

	puts("Hello, World!");
	return 0;
}
