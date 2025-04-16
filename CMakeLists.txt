cmake_minimum_required(VERSION 3.8)

set(PROJECT_NAME Lumin)
project(${PROJECT_NAME})

include_directories(include/)

add_executable(Lumin	src/main.cpp
						src/Window/Window.cpp
						src/Renderer/Renderer.cpp
						src/Shaders/Shaders.cpp
)

add_subdirectory(dependencies/glfw)
target_link_libraries(${PROJECT_NAME} glfw)

add_subdirectory(dependencies/glad)
target_link_libraries(${PROJECT_NAME} glad)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E copy_directory
					${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:${PROJECT_NAME}>)