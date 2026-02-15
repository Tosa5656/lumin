cmake_minimum_required(VERSION 4.0)

set(PROJECT_NAME lumin_engine)
project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 23)

set(PROJECT_SOURCES src/main.cpp
					src/window.cpp
					src/window_manager.cpp
					src/renderer.cpp
                    src/shaders.cpp)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
include_directories(${PROJECT_NAME} src/)

add_custom_command(
		TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
		COMMAND glslc "${CMAKE_CURRENT_SOURCE_DIR}/shaders/default.vert" -o "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/default_vert.spv"
		COMMAND glslc "${CMAKE_CURRENT_SOURCE_DIR}/shaders/default.frag" -o "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/default_frag.spv"
		COMMENT "Compiling shaders..."
)

target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)