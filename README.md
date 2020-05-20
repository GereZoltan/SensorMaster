# SensorMaster
Manager and logger for sensor modules

## Introduction
SensorMaster is remote controller/manager for SensorModule and other sensors.It provides high level control and logging.

## Device description

### Hardware
The software is developed and tested on Beaglebone black running the default Debian linux distribution.
SensorModule and other sensors are connected through I2C bus no. 2.
Used pins:
- I2C2_SCL on header P9 pin 19
- I2C2_SDA on header P9 pin 20

### Software
#### The master process
is the manager of the child processes.
Receives command from command line, socket connection or file (sent as command line argument).
Starts child processes.
Monitors the progress/state of the other processes.
- Writes to terminal
- Writes to log file
Sends commands to child processes.
Query statuses of the child processes.
When receives SIGINT, asks user for confirmation and
- gracefully terminates the other processes
- write termination status to the log file

Master process queries
- Status: measuring, error (1)
- Receives exit status

Master process sets
- Echoing to stdout on/off (2)
- Time interval of reading (3)
- Sends terminate signal (4)

#### The child processes
are reading data from
- sensor using
    - socket
    - serial port (I2C bus)
    - device
- SensorModule using
    - serial port (I2C bus)
Child processes also log measurement data to file.

Child process internal statuses and variables
- Status: measuring, error
- Echoing to stdout on/off (Can be set any time)
- Filename for measurement logging (Set at start)
- Sensor type: SensorModule NTC, SCC30-DB (Set at start)
- Sensor address (Set at start)
- Time interval of reading (Can be set any time)

Required methods and techniques:
- command line processing
- network sockets (UDP)
- file handling
- processes
- communication between processes
- signal handling
- I2C bus
- timer

## Further development
To be decided...
