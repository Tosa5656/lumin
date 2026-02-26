cmake_minimum_required(VERSION 3.10)
project(glad C)

add_library(glad STATIC src/gl.c)

target_include_directories(glad PUBLIC include)

if(NOT WIN32)
    target_link_libraries(glad PRIVATE ${CMAKE_DL_LIBS})
endif()
