/*
 * File:			ProcArgs.h
 *
 * Author:			Zoltan Gere
 * Created:			05/16/20
 * Description:		Library for command line arguments processing
 *
 * <MIT License>
 */

#ifndef PROCARGS_H
#define PROCARGS_H

#define MAXFILENAMELENGTH (32)

typedef struct {
	char sensorType[4];					// Sensor type: SensorModule NTC, SCC30-DB (Set at start)
	int sensorAddress;					// Sensor address (Set at start)
	char filename[MAXFILENAMELENGTH];	// Filename for measurement logging (Set at start)
	bool echo;							// Echoing to stdout on/off
	int interval;						// Time interval of reading (Can be set any time)
} ProcessArguments_t;

/**
 * @brief	Process the programs commnad line argument list.
 * 			Returns the number of configurations successfully read.
 *
 * @param	argc argument count
 * @param	argv argument string array
 * @param	mlfn Master log file name
 * @param	procArgs Process argument array
 *
 * @return	int
 *
 * Command line arguments:
 *		-h
 *		-c -l <master_logfile> -a <address_client_mode> -s <address_server_mode> -mfile <filename> -sensortype <NTC|SCC30> -sensoraddress <address> -echo {off|on} -interval <t>
 *		-f <inputfile_containing_command> -l <master_logfile> -a <address> -s <address>
 */
int ReadArgumentsFromCommandLine (int argc, char *argv[], char * mlfn, ProcessArguments_t *procArgs, int argBufSize );

#endif
