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
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "ProcArgs.h"

#define DEBUG 1

#define MAXPROCESSES (16)
#define PS_ERROR (-1)
#define PS_START (0)
#define PS_MEASURING (1)


// Constants
const char *defaultMasterLogfileName = "sensormaster.log";

// Global variables
volatile bool quitSignal = false;		// Quit signal, set by signal handler
char serverAddress[MAXFILENAMELENGTH];
int programMode = 0;					// 0 - Offline, 1 - Client, 2 - Server
struct timespec time_abs;
struct itimerspec timer_struct;
timer_t timerID;

static void XsigHandler ( int sigNo ) {
    if ( sigNo == SIGINT ) {
        quitSignal = true;
    }
    return;
}

static void rt_handler ( int signo ) {
    time_abs.tv_sec += 1;
    timer_struct.it_value = time_abs;
    timer_struct.it_interval.tv_sec = 0;
    timer_struct.it_interval.tv_nsec = 0;
    timer_settime ( timerID, TIMER_ABSTIME, &timer_struct, NULL );
    return;
}

/*
 *
 */
int main ( int argc, char *argv[] ) {
    // Process variables
    pid_t processes[MAXPROCESSES];
    int exitStatus;
    ProcessArguments_t procArgs[MAXPROCESSES];
    int processSocket[MAXPROCESSES][2];

    // Master process variables
    char masterLogfileName[MAXFILENAMELENGTH];
    FILE * masterLogfile;
    int configuredProcesses = 0;
    int runningProcesses = 0;
    int msg;
    char statusMsg[10];
    struct timespec currentTime;

    // Control variables
    bool exitSignal = false;

    // Signal handling variables
    struct sigaction Xhandler, oldHandler;
    sigset_t XSignalBlock;
    struct sigaction sigAct;
    sigset_t sigmask, timermask;
    struct sigevent tmrEvent;

    // Socket handling variables
    int serverSocket, server2ClientSocket;	// Sockets for server side handling
    int client2ServerSocket;				// Socket for client side handling
    struct sockaddr srvAddrStruct;
    int file_flags;

////////////////////////////////////////////////////////////////////////////////

    printf ( "SensorMaster\nVersion 0.1\n" );

#ifdef DEBUG
    printf ( "Debug mode!\n" );
#endif

    if ( ( argc > 1 ) && ( strcmp ( argv[1], "-h" ) == 0 ) ) {			// If help is invoked
        printf ( "Usage:\n" );										// Print usage and terminate
        printf ( "%s -h\n", argv[0] );
        printf ( "%s -c -l <master_logfile> -s <address> -mfile <filename> -sensortype <NTC|SCC30> -sensoraddress <address> -echo {0|1} -interval <t>\n", argv[0] );
        printf ( "%s -f -l <master_logfile> -s <address> inputfile_containing_command\n", argv[0] );
        printf ( "The master_logfile is optional. If not specified the default name is: %s\n", defaultMasterLogfileName );
        printf ( "-a <address> is optional. If specified the commands are sent to program running at <address>.\n" );
        printf ( "-s <address> is optional. If specified the program listening at <address> for commands. If neither -a or -s specified program works offline.\n" );
        printf ( "The format of inputfile is the same as in '-c' mode. One command per line. If the first character is # the line is ignored.\n" );
        printf ( "Sensor address format is hexadecimal with '0x' prefix, ie. 0xA8.\n" );
        exit ( 1 );
    }

    // Set up signal handler
    sigemptyset ( &XSignalBlock );
    sigaddset ( &XSignalBlock, SIGINT );
    Xhandler.sa_handler = XsigHandler;
    Xhandler.sa_mask = XSignalBlock;
    Xhandler.sa_flags = 0;
    if ( sigaction ( SIGINT, &Xhandler, &oldHandler ) < 0 ) {
        perror ( "Signal" );
        exit ( 1 );
    }

    // Set up timer signal handler
    sigAct.sa_flags = 0;
    sigAct.sa_handler = rt_handler;
    sigemptyset ( &sigmask );
    sigAct.sa_mask = sigmask;
    sigaction ( SIGRTMAX, &sigAct, NULL );

    tmrEvent.sigev_notify = SIGEV_SIGNAL;
    tmrEvent.sigev_signo = SIGRTMAX;
    tmrEvent.sigev_value.sival_int = 12;
    timer_create ( CLOCK_REALTIME, &tmrEvent, &timerID );

    sigfillset ( &timermask );
    sigdelset ( &timermask, SIGRTMAX );

    // Process program arguments
    configuredProcesses = ReadArgumentsFromCommandLine ( argc, argv, masterLogfileName, procArgs );

#ifdef DEBUG
    printf ( "Process count: %d\n", configuredProcesses );
#endif

#ifdef DEBUG
    printf ( "Server address: %s\n", serverAddress );
#endif

    // Check if client mode...
    // ...and open socket to address, send data and terminate.
    if ( programMode == 1 ) {
        srvAddrStruct.sa_family = AF_INET;
        strcpy ( srvAddrStruct.sa_data, serverAddress );
        client2ServerSocket = socket ( AF_INET, SOCK_STREAM, 0 );
        if ( client2ServerSocket == -1 ) {
            perror ( "clientsocket" );
            sigaction ( SIGINT, &oldHandler, NULL );						// Restore old signal handler
            exit ( EXIT_FAILURE );
        }

        if ( connect ( client2ServerSocket, &srvAddrStruct, sizeof ( srvAddrStruct ) ) == -1 ) {
            perror ( "clientconnect" );
            close ( client2ServerSocket );
            sigaction ( SIGINT, &oldHandler, NULL );						// Restore old signal handler
            exit ( EXIT_FAILURE );
        }
        for ( int i = 0; i < configuredProcesses; i++ ) {
            write ( client2ServerSocket, &procArgs[i], sizeof ( procArgs[i] ) );
        }
        close ( client2ServerSocket );
        sigaction ( SIGINT, &oldHandler, NULL );						// Restore old signal handler
        exit ( EXIT_SUCCESS );
    }

    // Check if server mode...
    // ...and start socket server, bind address and start listening
    if ( programMode == 2 ) {
        // Create address
        srvAddrStruct.sa_family = AF_INET;
        strcpy ( srvAddrStruct.sa_data, serverAddress );
        // Create server socket
        serverSocket = socket ( AF_INET, SOCK_STREAM, 0 );
        if ( serverSocket == -1 ) {
            perror ( "serversocket" );
            sigaction ( SIGINT, &oldHandler, NULL );						// Restore old signal handler
            exit ( EXIT_FAILURE );
        }
        file_flags = fcntl ( serverSocket, F_GETFL, 0 );					// Set to non-blocking mode
        file_flags |= O_NONBLOCK;
        fcntl ( serverSocket, F_SETFL, file_flags );

        // Check getaddrinfo

        // Bind socket to address
        if ( bind ( serverSocket, &srvAddrStruct, sizeof ( srvAddrStruct ) ) == -1 ) {
            perror ( "serverbind" );
            printf ( "%d\n", errno );
            close ( serverSocket );
            sigaction ( SIGINT, &oldHandler, NULL );						// Restore old signal handler
            exit ( EXIT_FAILURE );
        }
        // Listen
        if ( listen ( serverSocket, 10 ) == -1 ) {
            perror ( "serverlisten" );
            close ( serverSocket );
            sigaction ( SIGINT, &oldHandler, NULL );						// Restore old signal handler
            exit ( EXIT_FAILURE );
        }
    }

    clock_gettime ( CLOCK_REALTIME, &time_abs );
    time_abs.tv_sec += 1;
    timer_struct.it_value = time_abs;
    timer_struct.it_interval.tv_sec = 0;
    timer_struct.it_interval.tv_nsec = 0;
    timer_settime ( timerID, TIMER_ABSTIME, &timer_struct, NULL );

    masterLogfile = fopen ( masterLogfileName, "a+" );

#ifdef DEBUG
    printf ( "Init complete!\n" );
#endif
    while ( !exitSignal ) {
        if ( configuredProcesses > runningProcesses ) {
            // Start new process from process arguments
            if ( socketpair ( AF_UNIX, SOCK_STREAM, 0, processSocket[runningProcesses] ) == -1 ) {
                perror ( "socketpair" );
                close ( serverSocket );
                sigaction ( SIGINT, &oldHandler, NULL );						// Restore old signal handler
                exit ( EXIT_FAILURE );
            }
            processes[runningProcesses] = fork();
            if ( processes[runningProcesses] == 0 ) {				// Child process
                bool echo = procArgs[runningProcesses].echo;
                int measInterval = procArgs[runningProcesses].interval;
                int counter = 0;
                FILE *measLog;
                int meas;
                char unit = 'C';

                bool childTerminate = false;
                int childStatus = PS_START;

                measLog = fopen ( procArgs[runningProcesses].filename, "a+" );

                close ( processSocket[runningProcesses][1] );				// Child close 1

                childStatus = PS_MEASURING;
                while ( !childTerminate ) {
                    if ( measInterval == counter ) {
                        // Take measurement
                        meas = 24;
                        clock_gettime ( CLOCK_REALTIME, &currentTime );
                        // Log measurement
                        fprintf ( measLog, "%d, %d, %c\n", currentTime.tv_sec, meas, unit );
                        if ( echo ) {
                            printf ( "%d, %d, %c\n", currentTime.tv_sec, meas, unit );
                        }
                        counter = 0;
                    }
                    // Check command queue
                    read ( processSocket[runningProcesses][0], &msg, sizeof ( msg ) );
                    // Respond commands
                    if ( msg == 1 ) {
                        write ( processSocket[runningProcesses][0], &childStatus, sizeof ( childStatus ) );
                    }
                    if ( msg == 4 ) {
                        childTerminate = true;
                    }
                    counter++;
                }
                close ( processSocket[runningProcesses][0] );				// Child close 0
                fclose ( measLog );
                exit ( 0 );
            }	// End Child process
            close ( processSocket[runningProcesses][0] );				// Parent close 0
            runningProcesses++;
        }	// End start process

        // Check socket
        // and process command
        // increment configuredProcesses
        if ( programMode == 2 ) {
            server2ClientSocket = accept ( serverSocket, NULL, 0 );
            if ( server2ClientSocket == -1 ) {
                if ( ( errno == EAGAIN ) || ( errno == EWOULDBLOCK ) ) {
                    // No incoming connection. Do nothing
                } else {
                    perror ( "accept" );
                }
            } else {
                printf ( "Receiving command!\n" );
                while ( read ( server2ClientSocket, &procArgs[configuredProcesses], sizeof ( procArgs[configuredProcesses] ) ) != 0 ) {
                    configuredProcesses++;
                }

#ifdef DEBUG
                printf ( "Process count: %d\n", configuredProcesses );
#endif
            }
        }

        // Send status request to chidren
        msg = 1;
        for ( int i = 0; i < runningProcesses; i++ ) {
            write ( processSocket[i][1], &msg, sizeof ( msg ) );
        }

        // Wait for respond
        for ( int i = 0; i < runningProcesses; i++ ) {
            msg = 0;
            read ( processSocket[i][1], &msg, sizeof ( msg ) );
            switch ( msg ) {
            case PS_MEASURING:
                strcpy ( statusMsg, "Measuring\0" );
                break;
            case PS_ERROR:
                strcpy ( statusMsg, "Error\0" );
                break;
            default:
                strcpy ( statusMsg, "Unknown\0" );
            }
            // Log results to terminal and file
            clock_gettime ( CLOCK_REALTIME, &currentTime );
            fprintf ( masterLogfile, "%d %s\n", currentTime.tv_sec, statusMsg );
        }

        // Check quit status, ask user if really quit
        if ( quitSignal == true ) {
            printf ( "Really quit? (y)\n" );
            while ((msg = getchar()) == EOF)
				;
			printf("User answered: %c, %d\n", msg, msg);

            if ( toupper ( msg ) == 'Y' ) {
                // Send terminate signal to chidren
                msg = 4;
                for ( int i = 0; i < runningProcesses; i++ ) {
                    write ( processSocket[i][1], &msg, sizeof ( msg ) );
                }
                // write termination status to log file
                for ( int i = 0; i < runningProcesses; i++ ) {
                    wait ( &exitStatus );
                    if ( WIFEXITED ( exitStatus ) ) {
                        // log WEXITSTATUS(exitStatus)
                        clock_gettime ( CLOCK_REALTIME, &currentTime );
                        fprintf ( masterLogfile, "%d Process terminated with code: %d\n", currentTime.tv_sec, WEXITSTATUS ( exitStatus ) );
                    }
                }

                printf ( "\nReceived term signal. Quitting...\n" );
                exitSignal = true;
            } else {
                // User cancelled exit
                quitSignal = false;
            }
        }

        // Sleep until next timer (1 second)
        sigsuspend ( &timermask );
    }	// End while loop

    fclose ( masterLogfile );
    sigaction ( SIGINT, &oldHandler, NULL );						// Restore old signal handler
    return 0;
}
