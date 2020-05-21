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

#ifndef DEBUG
#define DEBUG 0
#endif

#define MAXLINELENGTH 128

extern char serverAddress[MAXFILENAMELENGTH];
// Constants
extern const char *defaultMasterLogfileName;
extern const char *defaultMeasurementLogfileName;
extern int programMode;					// 0 - Offline, 1 - Client, 2 - Server

int ReadArgumentsFromCommandLine ( int argc, char *argv[], char * mlfn, ProcessArguments_t *procArgs, int argBufSize ) {
    int processed = 0;
    bool commandInput = false;
    bool fileInput = false;
    FILE * settingsFile;
    char settingsFileName[MAXFILENAMELENGTH];
    char textRow[MAXLINELENGTH];
    char * ptok;
	int containsSetting;

    int digit = 0;

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
//             if ( argc > i + 1 ) {
//                 strncpy ( serverAddress, argv[i + 1], MAXFILENAMELENGTH );
//                 serverAddress[MAXFILENAMELENGTH - 1] = '\0';
//                 programMode = 2;
//             } else {
//                 printf ( "Missing server address, -s parameter is ignored.\n" );
//             }
            programMode = 2;
        }
    }

    if ( commandInput ) {
		containsSetting = 0;
        for ( int i = 2; i < argc; i++ ) {
            // Measurement log file
            if ( strcmp ( argv[i], "-mfile" ) == 0 ) {
                if ( argc > i + 1 ) {
                    strncpy ( procArgs[processed].filename, argv[i + 1], MAXFILENAMELENGTH );
                    procArgs[processed].filename[MAXFILENAMELENGTH - 1] = '\0';
                } else {
                    strncpy ( procArgs[processed].filename, defaultMeasurementLogfileName, MAXFILENAMELENGTH );
                    printf ( "Missing measurement filename! Default filename is used.\n" );
                }
            }
            // Sensor type
            if ( strcmp ( argv[i], "-sensortype" ) == 0 ) {
                if ( argc > i + 1 ) {
                    if ( strcmp ( argv[i+1], "NTC" ) == 0 ) {
                        strncpy ( procArgs[processed].sensorType, "NTC", 4 );
						containsSetting++;
                    } else if ( strcmp ( argv[i+1], "SCC" ) == 0 ) {
                        strncpy ( procArgs[processed].sensorType, "SCC", 4 );
						containsSetting++;
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
                                digit += argv[i+1][3] - '0';
                            } else if ( ( toupper ( argv[i+1][3] ) >= 'A' ) && ( toupper ( argv[i+1][3] ) <= 'F' ) ) {
                                digit += argv[i+1][3] - 'A' + 10;
                            }
                        }
                        procArgs[processed].sensorAddress = digit;
						containsSetting++;
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
        if (containsSetting == 2) {								// If valid setting found then keep the record
			processed++;
		}
    } else if ( fileInput ) {
        settingsFile = fopen ( settingsFileName, "r" );						// Open settings file
        if ( settingsFile == NULL ) {											// File open error
            perror ( "settingsfile" );
        } else {															// File successfully opened
            while (( fgets ( textRow, MAXLINELENGTH, settingsFile ) != NULL )
					&& ( processed < argBufSize)) {	// Read 1 row from file
                if ( textRow[0] == '#' )										// Skip comment lines
                    continue;
				containsSetting = 0;
                ptok = strtok ( textRow, " " );								// Process line
                while ( ptok != NULL ) {
                    // Measurement log file
                    if ( strcmp ( ptok, "-mfile" ) == 0 ) {
						ptok = strtok(NULL, " ");
                        if ( ptok != NULL ) {
                            strncpy ( procArgs[processed].filename, ptok, MAXFILENAMELENGTH );
                            procArgs[processed].filename[MAXFILENAMELENGTH - 1] = '\0';
                        } else {
							strncpy ( procArgs[processed].filename, defaultMeasurementLogfileName, MAXFILENAMELENGTH );
                            printf ( "Missing measurement filename! Default filename is used.\n" );
                        }
                    }
                    // Sensor type
                    if ( strcmp ( ptok, "-sensortype" ) == 0 ) {
						ptok = strtok(NULL, " ");
                        if ( ptok != NULL ) {
                            if ( strcmp ( ptok, "NTC" ) == 0 ) {
                                strncpy ( procArgs[processed].sensorType, "NTC", 4 );
								containsSetting++;
                            } else if ( strcmp ( ptok, "SCC" ) == 0 ) {
                                strncpy ( procArgs[processed].sensorType, "SCC", 4 );
								containsSetting++;
                            } else {
                                printf ( "Unknown sensor type!\n" );
                            }
                        } else {
                            memset ( procArgs[processed].sensorType, 0, 4 );
                            printf ( "Missing sensortype! -sensortype parameter is ignored.\n" );
                        }
                    }
                    // Sensor address
                    if ( strcmp ( ptok, "-sensoraddress" ) == 0 ) {
						ptok = strtok(NULL, " ");
                        if ( ptok != NULL ) {
                            if ( strlen ( ptok ) == 4 ) {
                                if ( ( ptok[0] == '0' ) && ( toupper ( ptok[1] ) == 'X' ) ) {
                                    if ( ( ptok[2] >= '0' ) && ( ptok[2] <= '9' ) ) {
                                        digit = ptok[2] - '0';
                                    } else if ( ( toupper ( ptok[2] ) >= 'A' ) && ( toupper ( ptok[2] ) <= 'F' ) ) {
                                        digit = ptok[2] - 'A' + 10;
                                    }
                                    digit *= 16;
                                    if ( ( ptok[3] >= '0' ) && ( ptok[3] <= '9' ) ) {
                                        digit += ptok[3] - '0';
                                    } else if ( ( toupper ( ptok[3] ) >= 'A' ) && ( toupper ( ptok[3] ) <= 'F' ) ) {
                                        digit += ptok[3] - 'A' + 10;
                                    }
                                }
                                procArgs[processed].sensorAddress = digit;
								containsSetting++;
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
                    if ( strcmp ( ptok, "-echo" ) == 0 ) {
						ptok = strtok(NULL, " ");
                        if ( ptok != NULL ) {
                            if ( strcmp ( ptok, "off" ) == 0 ) {
                                procArgs[processed].echo = false;
                            } else if ( strcmp ( ptok, "on" ) == 0 ) {
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
                    if ( strcmp ( ptok, "-interval" ) == 0 ) {
						ptok = strtok(NULL, " ");
                        if ( ptok != NULL ) {
                            sscanf ( ptok, "%d", &digit );
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
                    ptok = strtok(NULL, " ");
                }	// End of line processing
                if (containsSetting == 2) {								// If valid setting found then keep the record
					processed++;										// Otherwise next line will overwrite
				}
            }	// End of file reading
        }
    }	// End of file input

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
