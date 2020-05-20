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

extern char serverAddress[MAXFILENAMELENGTH];
// Constants
extern const char *defaultMasterLogfileName;
extern int programMode;					// 0 - Offline, 1 - Client, 2 - Server

int ReadArgumentsFromCommandLine ( int argc, char *argv[], char * mlfn, ProcessArguments_t *procArgs ) {
    int processed = 0;
    bool commandInput = false;
    bool fileInput = false;

    int digit = 0;

    memset ( serverAddress, 0, MAXFILENAMELENGTH );
    memset ( mlfn, 0, MAXFILENAMELENGTH );
    strncpy ( mlfn, defaultMasterLogfileName, strlen ( defaultMasterLogfileName ) );

    for ( int i = 1; i < argc; i++ ) {
        if ( strcmp ( argv[i], "-c" ) == 0 )
            commandInput = true;
        if ( strcmp ( argv[i], "-f" ) == 0 )
            fileInput = true;
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
            if ( argc > i + 1 ) {
                strncpy ( serverAddress, argv[i + 1], MAXFILENAMELENGTH );
                serverAddress[MAXFILENAMELENGTH - 1] = '\0';
                programMode = 2;
            } else {
                printf ( "Missing server address, -s parameter is ignored.\n" );
            }
        }
    }

    if ( commandInput ) {
        for ( int i = 2; i < argc; i++ ) {
            // Measurement log file
            if ( strcmp ( argv[i], "-mfile" ) == 0 ) {
                if ( argc > i + 1 ) {
                    strncpy ( procArgs[processed].filename, argv[i + 1], MAXFILENAMELENGTH );
                    procArgs[processed].filename[MAXFILENAMELENGTH - 1] = '\0';
                } else {
                    memset ( procArgs[processed].filename, 0, MAXFILENAMELENGTH );
                    printf ( "Missing measurement filename! -mfile parameter is ignored.\n" );
                }
            }
            // Sensor type
            if ( strcmp ( argv[i], "-sensortype" ) == 0 ) {
                if ( argc > i + 1 ) {
                    if ( strcmp ( argv[i+1], "NTC" ) == 0 ) {
                        strncpy ( procArgs[processed].sensorType, "NTC", 4 );
                    } else if ( strcmp ( argv[i+1], "SCC" ) == 0 ) {
                        strncpy ( procArgs[processed].sensorType, "SCC", 4 );
                    } else {
                        printf ( "Unknown sensor type!\n" );
                    }
                } else {
                    memset ( procArgs[processed].sensorType, 0, 4 );
                    printf ( "Missing sensortype! -sensortype parameter is ignored.\n" );
                }
            }
            // Sensor address
            if ( strcmp ( argv[i], "-sensoraddress" ) == 0 ) {
                if ( argc > i + 1 ) {
                    if ( strlen ( argv[i+1] ) == 4 ) {
                        if ( ( argv[i+1][0] == '0' ) && ( toupper ( argv[i+1][1] ) == 'X' ) ) {
                            if ( ( argv[i+1][2] >= '0' ) && ( argv[i+1][2] <= '9' ) ) {
                                digit = argv[i+1][2] - '0';
                            } else if ( ( toupper ( argv[i+1][2] ) >= 'A' ) && ( toupper ( argv[i+1][2] ) <= 'F' ) ) {
                                digit = argv[i+1][2] - 'A' + 10;
                            }
                            digit *= 16;
                            if ( ( argv[i+1][3] >= '0' ) && ( argv[i+1][3] <= '9' ) ) {
                                digit += argv[i+1][2] - '0';
                            } else if ( ( toupper ( argv[i+1][3] ) >= 'A' ) && ( toupper ( argv[i+1][3] ) <= 'F' ) ) {
                                digit += argv[i+1][2] - 'A' + 10;
                            }
                        }
                        procArgs[processed].sensorAddress = digit;
                    } else {
                        procArgs[processed].sensorAddress = 0;
                        printf ( "Error in sensor address! -sensoraddress parameter is ignored.\n" );
                    }
                } else {
                    procArgs[processed].sensorAddress = 0;
                    printf ( "Missing sensor address! -sensoraddress parameter is ignored.\n" );
                }
            }
            // Measurement echoing
            if ( strcmp ( argv[i], "-echo" ) == 0 ) {
                if ( argc > i + 1 ) {
                    if ( strcmp ( argv[i+1], "off" ) == 0 ) {
                        procArgs[processed].echo = false;
                    } else if ( strcmp ( argv[i+1], "on" ) == 0 ) {
                        procArgs[processed].echo = true;
                    } else {
                        procArgs[processed].echo = false;
                        printf ( "Error in echo parameter. -echo parameter is ignored.\n" );
                    }
                } else {
                    procArgs[processed].echo = false;
                    printf ( "Missing echo value! -echo parameter is ignored.\n" );
                }
            }

            // Measurement interval
            if ( strcmp ( argv[i], "-interval" ) == 0 ) {
                if ( argc > i + 1 ) {
                    sscanf ( argv[i+1], "%d", &digit );
                    if ( digit <= 0 ) {
                        printf ( "Time interval cannot be 0 or negative number. Minimum value 1 is used.\n" );
                        digit = 1;
                    }
                    procArgs[processed].interval = digit;
                } else {
                    procArgs[processed].interval = 1;
                    printf ( "Missing interval value! -interval parameter is ignored.\n" );
                }
            }
        }	// End of argument list processing
        processed++;
    } else if ( fileInput ) {

    }

#ifdef DEBUG
    printf ( "Master log filename: %s\n", mlfn );
    printf ( "Socket address: %s\n", serverAddress );
#endif

#ifdef DEBUG
    printf ( "Found %d process setting.\n", processed );
    for ( int i = 0; i < processed; i++ ) {
        printf ( "Sensor type: %s, sensor address: %d, filename: %s, echo: %d, interval: %d\n",
                 procArgs[i].sensorType, procArgs[i].sensorAddress, procArgs[i].filename, procArgs[i].echo, procArgs[i].interval );
    }
#endif

    return processed;
}
