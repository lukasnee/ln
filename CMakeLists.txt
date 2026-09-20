cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LN_FREERTOS "Enable FreeRTOS and features depending on it" OFF)
option(LN_LITTLEFS "Enable LittleFS support" OFF)
option(LN_LUA "Enable Lua scripting support" OFF)

set(LN_PORT
    ""
    CACHE STRING "")

project(
  ln
  VERSION 0.1.0
  LANGUAGES C CXX)

if(PROJECT_IS_TOP_LEVEL)
  if(CMAKE_CROSSCOMPILING)
    message(STATUS "Cross-compile configuration")
    set(LN_PORT "FreeRTOS")
    set(LN_FREERTOS ON)
    set(LN_LUA ON)
    set(FREERTOS_PORT
        GCC_ARM_CM7
        CACHE STRING "")
    set(FREERTOS_HEAP 4)
    add_library(freertos_config INTERFACE)
    target_include_directories(
      freertos_config
      INTERFACE ${CMAKE_CURRENT_LIST_DIR}/port/FreeRTOS/testing/include)
  else()
    message(STATUS "Unit test configuration")
    set(LN_PORT "stdcpp")
    include(FetchContent)
    FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG v3.11.0)
    FetchContent_MakeAvailable(Catch2)
    include(Catch)
    include(CTest)
    enable_testing()
  endif()
endif()

include(cmake/clang-tidy.cmake)

include(extern/fmt/fmt.cmake)
if(LN_FREERTOS)
  add_subdirectory(extern/FreeRTOS-Cpp)
endif()
if(LN_LITTLEFS)
  include(cmake/extern/littlefs.cmake)
endif()
if(LN_LUA)
  add_subdirectory(extern/lua)
endif()

add_subdirectory(port)

# NOTE: ln library is composed of multiple ln::* modular targets, the ln is an
# umbrella target that includes all of them for convenience.
add_library(ln INTERFACE)
add_library(ln::ln ALIAS ln)

add_subdirectory(ln)
