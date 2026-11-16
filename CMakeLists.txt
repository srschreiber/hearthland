cmake_minimum_required(VERSION 3.20)
project(Hearthland CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(raylib)

file(GLOB SRC CONFIGURE_DEPENDS src/*.cpp)
add_executable(hearthland ${SRC})
target_link_libraries(hearthland PRIVATE raylib)
target_compile_options(hearthland PRIVATE -Wall -Wno-missing-field-initializers)
if(APPLE)
  target_link_libraries(hearthland PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()
