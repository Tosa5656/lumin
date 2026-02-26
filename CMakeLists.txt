cmake_minimum_required(VERSION 3.20)

set(PROJECT_NAME lumin)
project(${PROJECT_NAME})

set(PROJECT_SOURCES src/main.cpp)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})