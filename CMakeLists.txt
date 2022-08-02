cmake_minimum_required(VERSION 3.8)
project(agbe)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_LIST_DIR}/x64)

set(CMAKE_CXX_STANDARD 23)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory("lib/glfw")
add_subdirectory("lib/SDL2-2.0.20")

set(SDL2_INCLUDE_DIR lib/SDL2-2.0.20/include)
set(IMGUI_INCLUDE_DIR lib/imgui)
set(GLAD_INCLUDE_DIR lib/glad/include)

include_directories(${SDL2_INCLUDE_DIR})
include_directories(${IMGUI_INCLUDE_DIR})
include_directories(${GLAD_INCLUDE_DIR})

file(GLOB SRC_FILES "agbe/*.cpp")
file(GLOB HEADER_FILES "agbe/*.h")
file(GLOB IMGUI_SRC "lib/imgui/*.cpp")
file(GLOB GLAD_SRC "lib/glad/src/*.c")
add_executable(agbe WIN32 ${SRC_FILES} ${HEADER_FILES} ${IMGUI_SRC} ${GLAD_SRC})
target_link_libraries(agbe SDL2-static)
target_link_libraries(agbe glfw)

target_compile_options(agbe PRIVATE "/Ox")
target_compile_options(agbe PRIVATE "/Oy")
target_compile_options(agbe PRIVATE "/O2")
target_compile_options(agbe PRIVATE "/GL")
target_link_options(agbe PRIVATE "/LTCG:INCREMENTAL")

source_group("Source Files" FILES ${SRC_FILES})
source_group("Header Files" FILES ${HEADER_FILES})
source_group("Dependencies" FILES ${IMGUI_SRC} ${GLAD_SRC})

set_target_properties(agbe PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}")
set_target_properties(agbe PROPERTIES VS_DEBUGGER_COMMAND "${EXECUTABLE_OUTPUT_PATH}/Release/agbe.exe")
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT agbe)