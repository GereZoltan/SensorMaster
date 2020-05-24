cmake_minimum_required(VERSION 3.0)

project(sensormaster LANGUAGES C)

add_executable(sensormaster sensormaster.c ProcArgs.c)
target_link_libraries(sensormaster rt)

install(TARGETS sensormaster RUNTIME DESTINATION bin)

# Cross-compile
#set(CMAKE_SYSTEM_NAME beaglebone-linux)
#set(CMAKE_SYSTEM_PROCESSOR armhf)

#set(CMAKE_C_COMPILER /usr/bin/arm-linux-gnueabihf-gcc)
#set(CMAKE_CXX_COMPILER /usr/bin/arm-linux-gnueabihf-g++)
