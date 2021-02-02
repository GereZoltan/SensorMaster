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

/*
 * TODO:
 * You could use inet_ntop() to convert ip-address to a string. Your conversion works only with processors little endian byte order. The library functions work on all linuxes regardless of the byte order.
 *
 * You have also some of your own byte swapping routines that depend on the endianness of the processor. With CMAKE you can use TestBigEndian to determine the endianness of your system and have your code to swap bytes when needed.
 *
 * The main function has comments to divide it into sections but I still feel that using functions would have improved the readability of the code. Function calls instead of blocks of code help in seeing the overall structure of the program. The current version requires scrolling back and forth to get idea of the program flow in general and in different operating modes.
 *
 */

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "ProcArgs.h"

//#ifndef DEBUG
//#define DEBUG 1
//#endif

#define MAXPROCESSES (16)
#define PS_ERROR (-1)
#define PS_START (0)
#define PS_MEASURING (1)

#define MYPORT "4950"	// the port users will be connecting to

// Constants
const char *defaultMasterLogfileName = "sensormaster.log";
const char *defaultMeasurementLogfileName = "measurement.txt";

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

/**
 * @brief get IPv4 or IPv6 address in human readable format
 *
 * @param sa		socket address structure
 * @param strIPaddr	string containing the result
 * @return void*
 */
void get_ip_addr ( struct sockaddr *sa, char * strIPaddr ) {
	uint32_t addrv4;
	char * ptr = strIPaddr;
    if ( sa->sa_family == AF_INET ) {
		addrv4 = ( ( struct sockaddr_in* ) sa )->sin_addr.s_addr;
		snprintf(strIPaddr, 40, "%d.%d.%d.%d", addrv4 & 255, (addrv4 >> 8) & 255 , (addrv4 >> 16) & 255 , addrv4 >> 24 );
    } else {
	for (int i = 0; i < 8; i++) {
		sprintf(ptr, "%x",  ( ( struct sockaddr_in6* ) sa )->sin6_addr.__in6_u.__u6_addr16[i] );
		ptr +=4;
		*ptr = ':';
		ptr++;
	}
	*ptr = '\0';
	}

}

/**
 * @brief get current time in human readable format
 *
 * @param timeStr	string containing the result
 * @param len		string buffer size
 * @return void*
 */
void getTimeStr( char * timeStr, size_t len) {
	struct timespec currentTime;

	clock_gettime ( CLOCK_REALTIME, &currentTime );
	snprintf(timeStr, len, "%s", ctime ( &currentTime.tv_sec ) );
	for (int i = 0; i < strlen(timeStr); i++) {
		if ((timeStr[i] == '\r') || (timeStr[i] == '\n')) {
			timeStr[i] = '\0';
		}
	}
}

/**
 * @brief main function
 *
 * @param argc argument count
 * @param argv argument list
 * @return int
 */
int main ( int argc, char *argv[] ) {
    //////////////////////////////////////// Process variables
    pid_t processes[MAXPROCESSES];				// Process list
    int exitStatus;
    ProcessArguments_t procArgs[MAXPROCESSES];	// Process arguments
    int processSocket[MAXPROCESSES][2];			// Communication channel between process and master

    //////////////////////////////////////// Master process variables
    char masterLogfileName[MAXFILENAMELENGTH];
    FILE * masterLogfile;
    int configuredProcesses = 0;
    int runningProcesses = 0;
    int msg;									// Command to send for processes
    char statusMsg[10];							// Status report from process
    char timestamp[40];							// Time stamp
	char strIPAddr[40];							// Holds the IP address in string format

    //////////////////////////////////////// Control variables
    bool exitSignal = false;

    //////////////////////////////////////// Signal handling variables
    struct sigaction Xhandler, oldHandler;
    sigset_t XSignalBlock;
    struct sigaction sigAct;
    sigset_t sigmask, timermask;
    struct sigevent tmrEvent;

    // Socket handling variables
    int serverSocket, server2ClientSocket;		// Sockets for server side handling
    int client2ServerSocket;					// Socket for client side handling
    struct sockaddr srvAddrStruct, clientAddrStruct;
    int file_flags;
    struct addrinfo hints, *servinfo, *p;
    int rv;
	socklen_t addressStructSize;

	//////////////////////////////////////// Program start

    printf ( "SensorMaster\nVersion 0.1\n" );

#ifdef DEBUG
    printf ( "Debug mode!\n" );
#endif

    if ( ( argc > 1 ) && ( strcmp ( argv[1], "-h" ) == 0 ) ) {			// If help is invoked
        printf ( "Usage:\n" );											// Print usage and terminate
        printf ( "%s -h\n", argv[0] );
        printf ( "%s -c [-l <master_logfile>] [-a <address> | -s] [-mfile <filename>] -sensortype <NTC|SCC30> -sensoraddress <address> [-echo {off|on} -interval <t>]\n", argv[0] );
        printf ( "%s -f <inputfile_containing_command> [-l <master_logfile>] [-a <address> | -s]\n", argv[0] );
        printf ( "-l <master_logfile> is optional. If not specified the default name is: %s\n", defaultMasterLogfileName );
        printf ( "-a <address> is optional. If specified the commands are sent to program running at <address>.\n" );
        printf ( "-s is optional. If specified the program listening on network for commands.\n" );
        printf ( "If neither -a or -s specified program works offline.\n" );
        printf ( "The format of inputfile is the same as in '-c' mode. One command per line. If the first character of line is '#' the line is ignored.\n" );
        printf ( "Sensor address format is hexadecimal with '0x' prefix, ie. 0xA8.\n" );
        exit ( 1 );
    }

    // Process program arguments
    configuredProcesses = ReadArgumentsFromCommandLine ( argc, argv, masterLogfileName, procArgs, MAXPROCESSES );
    masterLogfile = fopen ( masterLogfileName, "a+" );

    // Set up signal handler
    sigemptyset ( &XSignalBlock );
    sigaddset ( &XSignalBlock, SIGINT );
    Xhandler.sa_handler = XsigHandler;
    Xhandler.sa_mask = XSignalBlock;
    Xhandler.sa_flags = 0;
    if ( sigaction ( SIGINT, &Xhandler, &oldHandler ) < 0 ) {
        perror ( "Signal" );
		getTimeStr(timestamp, sizeof(timestamp));
        fprintf ( masterLogfile, "%s, %s, %s\n", timestamp, "Signal", strerror ( errno ) );
        fclose ( masterLogfile );
        exit ( EXIT_FAILURE );
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

    sigfillset ( &timermask );										// used by sigsuspend at the end of main loop
    sigdelset ( &timermask, SIGRTMAX );

#ifdef DEBUG
    printf ( "Process count: %d\n", configuredProcesses );
    printf ( "Server address: %s\n", serverAddress );
#endif

	//////////////////////////////////////// Program in client mode
	//////////////////////////////////////// Sends commands to server and terminates

    // Check if client mode...
    // ...and open socket to address, send data and terminate.
    if ( programMode == 1 ) {
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = 0;
		hints.ai_protocol = 0;

		if ((rv = getaddrinfo(serverAddress, MYPORT, &hints, &servinfo)) != 0) {
			printf("getaddrinfo: %s\n", gai_strerror(rv));
			getTimeStr(timestamp, sizeof(timestamp));
            fprintf ( masterLogfile, "%s, %s, %s\n", timestamp, "clientsocket", gai_strerror(rv) );
            fclose ( masterLogfile );
            sigaction ( SIGINT, &oldHandler, NULL );						// Restore old signal handler
            exit ( EXIT_FAILURE );
			return 1;
		}

#ifdef DEBUG
        printf("getaddrinfo returned:\n");
		for ( p = servinfo; p != NULL; p = p->ai_next ) {
			get_ip_addr(p->ai_addr, strIPAddr);
			printf("flags: %d\tfamily:%d\tsocktype:%d\tprotocol:%d\taddr:%s\tcanonname:%s\n",
//					p->ai_flags, p->ai_family, p->ai_socktype, p->ai_protocol, p->ai_addr->sa_data , p->ai_canonname);
					p->ai_flags, p->ai_family, p->ai_socktype, p->ai_protocol, strIPAddr , p->ai_canonname);
		}
		printf("End of listing\n");
#endif

		// loop through all the results and make a socket
		for(p = servinfo; p != NULL; p = p->ai_next) {
			if ((client2ServerSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				perror("clientsocket");
				continue;
			}
			if (connect(client2ServerSocket, p->ai_addr, p->ai_addrlen) != -1)
				break;
			close(client2ServerSocket);
		}

		if (p == NULL) {
			printf("Connection failed\n");
            getTimeStr(timestamp, sizeof(timestamp));
            fprintf ( masterLogfile, "%s, %s\n", timestamp, "Connection failed" );
            fclose ( masterLogfile );
            sigaction ( SIGINT, &oldHandler, NULL );						// Restore old signal handler
            exit ( EXIT_FAILURE );
		}

        for ( int i = 0; i < configuredProcesses; i++ ) {
            write ( client2ServerSocket, &procArgs[i], sizeof ( procArgs[i] ) );
        }

        getTimeStr(timestamp, sizeof(timestamp));
        fprintf ( masterLogfile, "%s, %s\n", timestamp, "Command send successful" );
        fclose ( masterLogfile );
        close ( client2ServerSocket );
		freeaddrinfo(servinfo);
        sigaction ( SIGINT, &oldHandler, NULL );						// Restore old signal handler
        exit ( EXIT_SUCCESS );
    }

	//////////////////////////////////////// Program in server mode
	//////////////////////////////////////// Set up server, and start listening

    // Check if server mode...
    // ...and start socket server, bind address and start listening
    if ( programMode == 2 ) {
        memset ( &hints, 0, sizeof (struct addrinfo) );
        hints.ai_family = AF_UNSPEC;								// set to AF_INET to force IPv4
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;								// use my IP
        hints.ai_protocol = 0;
		hints.ai_addr = NULL;
		hints.ai_next = NULL;

        if ( ( rv = getaddrinfo ( NULL, MYPORT, &hints, &servinfo ) ) != 0 ) {
            printf ( "getaddrinfo: %s\n", gai_strerror ( rv ) );
            getTimeStr(timestamp, sizeof(timestamp));
            fprintf ( masterLogfile, "%s, %s, %s\n", timestamp, "getaddrinfo", gai_strerror ( rv ) );
            fclose ( masterLogfile );
            sigaction ( SIGINT, &oldHandler, NULL );				// Restore old signal handler
            exit ( EXIT_FAILURE );
        }

#ifdef DEBUG
        printf("getaddrinfo returned:\n");
		for ( p = servinfo; p != NULL; p = p->ai_next ) {
			printf("flags: %d\tfamily:%d\tsocktype:%d\tprotocol:%d\taddr:%s\tcanonname:%s\n",
				   p->ai_flags, p->ai_family, p->ai_socktype, p->ai_protocol, p->ai_addr->sa_data , p->ai_canonname);
		}
		printf("End of listing\n");
#endif

        // loop through all the results and bind to the first we can
        for ( p = servinfo; p != NULL; p = p->ai_next ) {
            if ( ( serverSocket = socket ( p->ai_family, p->ai_socktype,
                                           p->ai_protocol ) ) == -1 ) {
                perror ( "listener: socket" );
                continue;
            }

            if ( bind ( serverSocket, p->ai_addr, p->ai_addrlen ) == 0 ) {
                break;
            }

            close ( serverSocket );
        }

        if ( p == NULL ) {
            printf ( "listener: failed to bind socket\n" );
            getTimeStr(timestamp, sizeof(timestamp));
            fprintf ( masterLogfile, "%s, %s\n", timestamp, "listener: failed to bind socket\n" );
            fclose ( masterLogfile );
            sigaction ( SIGINT, &oldHandler, NULL );						// Restore old signal handler
            exit ( EXIT_FAILURE );
        }

        freeaddrinfo ( servinfo );

        file_flags = fcntl ( serverSocket, F_GETFL, 0 );					// Set to non-blocking mode
        file_flags |= O_NONBLOCK;
        fcntl ( serverSocket, F_SETFL, file_flags );

        // Listen
        if ( listen ( serverSocket, 10 ) == -1 ) {
            perror ( "serverlisten" );
            getTimeStr(timestamp, sizeof(timestamp));
            fprintf ( masterLogfile, "%s, %s, %s\n", timestamp, "serverlisten", strerror ( errno ) );
            fclose ( masterLogfile );
            close ( serverSocket );
            sigaction ( SIGINT, &oldHandler, NULL );						// Restore old signal handler
            exit ( EXIT_FAILURE );
        }

        addressStructSize = sizeof(srvAddrStruct);
        getsockname(serverSocket, &srvAddrStruct, &addressStructSize);
		get_ip_addr(&srvAddrStruct, strIPAddr);
		printf("Server listening on address: %s\n", strIPAddr );
		printf("Port number: %s\n", MYPORT );
    }

	//////////////////////////////////////// Set up timer

    // Set up timer
    clock_gettime ( CLOCK_REALTIME, &time_abs );
    time_abs.tv_sec += 1;
    timer_struct.it_value = time_abs;
    timer_struct.it_interval.tv_sec = 0;
    timer_struct.it_interval.tv_nsec = 0;
    timer_settime ( timerID, TIMER_ABSTIME, &timer_struct, NULL );

#ifdef DEBUG
    printf ( "Init complete!\n" );
#endif

	//////////////////////////////////////// Start main program loop

    while ( !exitSignal ) {

		//////////////////////////////////////// Start new processes

        if ( configuredProcesses > runningProcesses ) {
            // Start new process from process arguments
            if ( socketpair ( AF_UNIX, SOCK_STREAM, 0, processSocket[runningProcesses] ) == -1 ) {
                perror ( "socketpair" );
                getTimeStr(timestamp, sizeof(timestamp));
                fprintf ( masterLogfile, "%s, %s, %s\n", timestamp, "socketpair", strerror ( errno ) );
                fclose ( masterLogfile );
                close ( serverSocket );
                sigaction ( SIGINT, &oldHandler, NULL );						// Restore old signal handler
                exit ( EXIT_FAILURE );
            }

            //////////////////////////////////////// Child process starts

            processes[runningProcesses] = fork();
            if ( processes[runningProcesses] == 0 ) {				// Child process
                bool echo = procArgs[runningProcesses].echo;
                int measInterval = procArgs[runningProcesses].interval;
                int counter = 0;
                FILE *measLog;
                int16_t meas = 0;
                char unit = '\0';
                char senstype = '\0';
                int sensorFD;
				char reg;

                bool childTerminate = false;
                int childStatus = PS_START;

                measLog = fopen ( procArgs[runningProcesses].filename, "a+" );

                close ( processSocket[runningProcesses][1] );				// Child close socket side 1

				if (strcmp(procArgs[runningProcesses].sensorType, "NTC") == 0) {
					// Init NTC sensor
					sensorFD = open("/dev/i2c-2", O_RDWR);
					if (sensorFD == -1) {
						perror("i2c_open");
						getTimeStr(timestamp, sizeof(timestamp));
						fprintf ( measLog, "%s, %s, %s\n", timestamp, "i2c_open", strerror ( errno ) );
					}
					if (ioctl(sensorFD, I2C_SLAVE, procArgs[runningProcesses].sensorAddress) == -1) {
						perror("i2c_ioctl");
						getTimeStr(timestamp, sizeof(timestamp));
						fprintf ( measLog, "%s, %s, %s\n", timestamp, "i2c_ioctl", strerror ( errno ) );
					}
				}

                childStatus = PS_MEASURING;
                while ( !childTerminate ) {
                    if ( measInterval == counter ) {
                        // Take measurement
						// Read value
						reg = 1;
						write(sensorFD, &reg, 1);
						if (read(sensorFD, &meas, 2) <= 0) {
							childStatus = PS_ERROR;
						} else {
							childStatus = PS_MEASURING;
						}
						meas = (meas >> 8) | (meas << 8);			// Swap byte order
						// Read type
						reg = 2;
						write(sensorFD, &reg, 1);
						read(sensorFD, &senstype, 1);
						// Read unit
						reg = 3;
						write(sensorFD, &reg, 1);
						read(sensorFD, &unit, 1);
                        getTimeStr(timestamp, sizeof(timestamp));
                        // Log measurement
                        fprintf ( measLog, "%s, %d, %c\n", timestamp, meas, unit );
                        if ( echo ) {
//                             printf ( "%s, Type: %c Value: %d, %x Unit: %c, %x\n", ctime ( &currentTime.tv_sec ), senstype, meas, meas, unit, unit );
							printf ( "%s, Value: %d\tUnit: %c\n", timestamp, meas, unit );
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

                close(sensorFD);
                close ( processSocket[runningProcesses][0] );				// Child close socket side 0
                fclose ( measLog );
                exit ( EXIT_SUCCESS );
            }	// End Child process

            close ( processSocket[runningProcesses][0] );				// Parent close socket side 0
            runningProcesses++;
        }	// End start process

        //////////////////////////////////////// Server accepting commands

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
                    getTimeStr(timestamp, sizeof(timestamp));
                    fprintf ( masterLogfile, "%s, %s, %s\n", timestamp, "accept", strerror ( errno ) );
                }
            } else {
                printf ( "Receiving command!\n" );
                while ( read ( server2ClientSocket, &procArgs[configuredProcesses], sizeof ( procArgs[configuredProcesses] ) ) != 0 ) {
                    configuredProcesses++;
                }

#ifdef DEBUG
                printf ( "Process count: %d\n", configuredProcesses );
#endif

				addressStructSize = sizeof(clientAddrStruct);
				getpeername(server2ClientSocket, &clientAddrStruct, &addressStructSize);
				get_ip_addr(&clientAddrStruct, strIPAddr);
				printf("Received command from: %s\n", strIPAddr);
                getTimeStr(timestamp, sizeof(timestamp));
                fprintf ( masterLogfile, "%s, Received command from: %s\n", timestamp, strIPAddr );
            }
        }

        //////////////////////////////////////// Query children's status

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
            getTimeStr(timestamp, sizeof(timestamp));
            fprintf ( masterLogfile, "%s %s\n", timestamp, statusMsg );
			printf ( "%s %s\n", timestamp, statusMsg );
        }	// End wait for respond

        //////////////////////////////////////// Check quit status

        // Check quit status, ask user if really quit
        if ( quitSignal == true ) {
            printf ( "Really quit? (y)\n" );
            while ( ( msg = getchar() ) == EOF )
                ;
//             printf ( "User answered: %c, %d\n", msg, msg );

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
                        getTimeStr(timestamp, sizeof(timestamp));
                        fprintf ( masterLogfile, "%s Process terminated normally with code: %d\n", timestamp, WEXITSTATUS ( exitStatus ) );
                    }
                    if ( WIFSIGNALED ( exitSignal ) ) {
                        getTimeStr(timestamp, sizeof(timestamp));
                        fprintf ( masterLogfile, "%s Process terminated abnormally by signal: %d\n", timestamp, WTERMSIG ( exitStatus ) );
                    }
                }

                printf ( "\nReceived term signal. Quitting...\n" );
                exitSignal = true;
            } else {
                // User cancelled exit
                quitSignal = false;
            }
        }	// End quit signal check

        // Sleep until next timer (1 second)
        sigsuspend ( &timermask );
    }	// End while loop

    //////////////////////////////////////// Final clean-up

    fclose ( masterLogfile );
    sigaction ( SIGINT, &oldHandler, NULL );						// Restore old signal handler
    return 0;
}	// End of main function
