cmake_minimum_required(VERSION 3.0)

project(sensormaster LANGUAGES C)

add_executable(sensormaster main.c ProcArgs.c)
target_link_libraries(sensormaster rt)

install(TARGETS sensormaster RUNTIME DESTINATION bin)
