cmake_minimum_required(VERSION 4.0)

set(PROJECT_NAME lumin_engine)
project(${PROJECT_NAME})

set(PROJECT_SOURCES src/main.cpp)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
include_directories(${PROJECT_NAME} src/)

add_subdirectory(glfw)

set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "")
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "")
set(GLFW_INSTALL OFF CACHE INTERNAL "")

target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
include_directories(${PROJECT_NAME} glfw/include)

find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)