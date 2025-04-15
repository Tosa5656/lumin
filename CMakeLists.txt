cmake_minimum_required (VERSION 3.8)

set(PROJECT_NAME Lumin)
project (${PROJECT_NAME})

add_executable (Lumin src/main.cpp)

add_subdirectory(dependencies/glfw)
target_link_libraries(${PROJECT_NAME} glfw)

add_subdirectory(dependencies/glad)
target_link_libraries(${PROJECT_NAME} glad)