/*
 * File:			ProcArgs.c
 *
 * Author:			Zoltan Gere
 * Created:			05/16/20
 * Description:		Library for command line arguments processing
 *
 * <MIT License>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "ProcArgs.h"

// #ifndef DEBUG
// #define DEBUG 1
// #endif

#define MAXLINELENGTH 128

extern char serverAddress[MAXFILENAMELENGTH];
// Constants
extern const char *defaultMasterLogfileName;
extern const char *defaultMeasurementLogfileName;
extern int programMode;					// 0 - Offline, 1 - Client, 2 - Server

/**
 * @brief Process one line of parameters
 *
 * @param textRow   whitespace separated parameters
 * @param procArg   processed settings
 * @return int      Number of required parameters successfully read
 */
static int ProcessLine ( char * textRow, ProcessArguments_t * procArg ) {
    char * ptok;
    int containsSetting = 0;

    ptok = strtok ( textRow, " " );								// Process line
    while ( ptok != NULL ) {
        // Measurement log file
        if ( strcmp ( ptok, "-mfile" ) == 0 ) {
            ptok = strtok ( NULL, " " );
            if ( ptok != NULL ) {
                strncpy ( procArg->filename, ptok, MAXFILENAMELENGTH );
                procArg->filename[MAXFILENAMELENGTH - 1] = '\0';
            } else {
                strncpy ( procArg->filename, defaultMeasurementLogfileName, MAXFILENAMELENGTH );
                printf ( "Missing measurement filename! Default filename is used.\n" );
            }
        }
        // Sensor type
        if ( strcmp ( ptok, "-sensortype" ) == 0 ) {
            ptok = strtok ( NULL, " " );
            if ( ptok != NULL ) {
                if ( strcmp ( ptok, "NTC" ) == 0 ) {
                    strncpy ( procArg->sensorType, "NTC", 4 );
                    containsSetting++;
                } else if ( strcmp ( ptok, "SCC" ) == 0 ) {
                    strncpy ( procArg->sensorType, "SCC", 4 );
                    containsSetting++;
                } else {
                    printf ( "Unknown sensor type!\n" );
                }
            } else {
                memset ( procArg->sensorType, 0, 4 );
                printf ( "Missing sensortype! -sensortype parameter is ignored.\n" );
            }
        }
        // Sensor address
        if ( strcmp ( ptok, "-sensoraddress" ) == 0 ) {
            procArg->sensorAddress = 0;
            ptok = strtok ( NULL, " " );
            if ( ptok != NULL ) {
                if ( strlen ( ptok ) <= 4 ) {
                    sscanf ( ptok, "%x", &procArg->sensorAddress );
                    if ( procArg->sensorAddress != 0 ) {
                        containsSetting++;
                    } else {
                        printf ( "Error in sensor address! -sensoraddress parameter is ignored.\n" );
                    }
                } else {
                    printf ( "Error in sensor address! -sensoraddress parameter is ignored.\n" );
                }
            } else {
                printf ( "Missing sensor address! -sensoraddress parameter is ignored.\n" );
            }
        }
        // Measurement echoing
        if ( strcmp ( ptok, "-echo" ) == 0 ) {
            ptok = strtok ( NULL, " " );
            if ( ptok != NULL ) {
                if ( strcmp ( ptok, "off" ) == 0 ) {
                    procArg->echo = false;
                } else if ( strcmp ( ptok, "on" ) == 0 ) {
                    procArg->echo = true;
                } else {
                    procArg->echo = false;
                    printf ( "Error in echo parameter. -echo parameter is ignored.\n" );
                }
            } else {
                procArg->echo = false;
                printf ( "Missing echo value! -echo parameter is ignored.\n" );
            }
        }

        // Measurement interval
        if ( strcmp ( ptok, "-interval" ) == 0 ) {
            ptok = strtok ( NULL, " " );
            if ( ptok != NULL ) {
                sscanf ( ptok, "%d", &procArg->interval );
                if ( procArg->interval <= 0 ) {
                    printf ( "Time interval cannot be 0 or negative number. Minimum value 1 is used.\n" );
                    procArg->interval = 1;
                }
            } else {
                procArg->interval = 1;
                printf ( "Missing interval value! -interval parameter is ignored.\n" );
            }
        }
        ptok = strtok ( NULL, " " );
    }   // End of line processing
    return containsSetting;
}

/**
 * @brief   Process the input parameters
 *          Read command line arguments
 *          If specified, read from file
 *          Returns the number of configurations successfully read.
 *
 * @param   argc argument count
 * @param   argv argument string array
 * @param   mlfn Master log file name
 * @param   procArgs Process argument array
 *
 * @return   int
 *
 * Command line arguments:
 *      -h
 *      -c -l <master_logfile> -a <address_client_mode> -s -mfile <filename> -sensortype <NTC|SCC30> -sensoraddress <address> -echo {off|on} -interval <t>
 *      -f <inputfile_containing_command> -l <master_logfile> -a <address> -s <address>
 */
int ReadArgumentsFromCommandLine ( int argc, char *argv[], char * mlfn, ProcessArguments_t * procArgs, int argBufSize ) {
    int processed = 0;
    bool commandInput = false;
    bool fileInput = false;
    FILE * settingsFile;
    char settingsFileName[MAXFILENAMELENGTH];
    char textRow[MAXLINELENGTH];

    memset ( serverAddress, 0, MAXFILENAMELENGTH );
    memset ( mlfn, 0, MAXFILENAMELENGTH );
    strncpy ( mlfn, defaultMasterLogfileName, strlen ( defaultMasterLogfileName ) );

    for ( int i = 1; i < argc; i++ ) {
        if ( strcmp ( argv[i], "-c" ) == 0 )
            commandInput = true;
        if ( strcmp ( argv[i], "-f" ) == 0 ) {
            fileInput = true;
            if ( argc > i + 1 ) {
                strncpy ( settingsFileName, argv[i + 1], MAXFILENAMELENGTH );
                settingsFileName[MAXFILENAMELENGTH - 1] = '\0';
            } else {
                printf ( "Missing settings filename!\n" );
                fileInput = false;
            }
        }
        if ( strcmp ( argv[i], "-l" ) == 0 ) {
            if ( argc > i + 1 ) {
                strncpy ( mlfn, argv[i + 1], MAXFILENAMELENGTH );
                mlfn[MAXFILENAMELENGTH - 1] = '\0';
            } else {
                printf ( "Missing log filename! Using default name.\n" );
            }
        }
        // Client mode
        if ( strcmp ( argv[i], "-a" ) == 0 ) {
            if ( argc > i + 1 ) {
                strncpy ( serverAddress, argv[i + 1], MAXFILENAMELENGTH );
                serverAddress[MAXFILENAMELENGTH - 1] = '\0';
                programMode = 1;
            } else {
                printf ( "Missing server address, -s parameter is ignored.\n" );
            }
        }
        // Server mode
        if ( strcmp ( argv[i], "-s" ) == 0 ) {
            programMode = 2;
        }
    }

    if ( commandInput ) {
        textRow[0] = '\0';

        // Convert command line arguments to textRow readable string format
        for ( int i = 1; i < argc; i++ ) {
            strcat ( textRow, argv[i] );
            strcat ( textRow, " " );
        }

#ifdef DEBUG
        printf ( "Concat. arguments : %s\n", textRow );
#endif

        if ( ProcessLine ( textRow, &procArgs[processed] ) == 2 ) {         // There are 2 required parameters
            processed++;                                                    // If valid setting found then keep the record
        }
    } else if ( fileInput ) {
        settingsFile = fopen ( settingsFileName, "r" );                     // Open settings file
        if ( settingsFile == NULL ) {                                       // File open error
            perror ( "settingsfile" );
        } else {                                                            // File successfully opened
            while ( ( fgets ( textRow, MAXLINELENGTH, settingsFile ) != NULL )
                    && ( processed < argBufSize ) ) {	// Read 1 row from file
                if ( textRow[0] == '#' )                                    // Skip comment lines
                    continue;
                if ( ProcessLine ( textRow, &procArgs[processed] ) == 2 ) { // There are 2 required parameters
                    processed++;                                            // If valid setting found then keep the record
                }
            }   // End of file reading
        }
    }   // End of file input

#ifdef DEBUG
    printf ( "Master log filename: %s\n", mlfn );
    printf ( "Socket address: %s\n", serverAddress );
    printf ( "Found %d process setting.\n", processed );
    for ( int i = 0; i < processed; i++ ) {
        printf ( "Sensor type: %s, sensor address: %d, filename: %s, echo: %d, interval: %d\n",
                 procArgs[i].sensorType, procArgs[i].sensorAddress, procArgs[i].filename, procArgs[i].echo, procArgs[i].interval );
    }
#endif

    return processed;
}
