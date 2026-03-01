cmake_minimum_required(VERSION 3.20)

set(PROJECT_NAME lumin)
project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PROJECT_SOURCES src/main.cpp
					src/window.cpp
					src/renderer.cpp
					src/shaders.cpp)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
		glm
		GIT_REPOSITORY https://github.com/g-truc/glm.git
		GIT_TAG 1.0.1
		GIT_SHALLOW TRUE
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLM_QUIET ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw glm)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})
include_directories(${PROJECT_NAME} src/)

add_subdirectory(glad/)

target_link_libraries(${PROJECT_NAME} PRIVATE
	glfw
    glad
	glm::glm
)