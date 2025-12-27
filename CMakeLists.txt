cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LN_FREERTOS "Enable FreeRTOS and features depending on it" OFF)
option(LN_LUA "Enable Lua scripting support" OFF)
option(LN_LITTLEFS "Enable LittleFS support" OFF)

project(
  ln
  VERSION 0.1.0
  LANGUAGES C CXX)

set(CROSSCOMPILE_TEST false)
if(PROJECT_IS_TOP_LEVEL AND CMAKE_CROSSCOMPILING)
  set(CROSSCOMPILE_TEST true)
endif()

if(CROSSCOMPILE_TEST)
  set(LN_FREERTOS ON)
  set(FREERTOS_PORT
      GCC_ARM_CM7
      CACHE STRING "")
  set(FREERTOS_HEAP 4)
  add_library(freertos_config INTERFACE)
  target_include_directories(
    freertos_config INTERFACE ${CMAKE_CURRENT_LIST_DIR}/tests/config/include)
endif()

include(cmake/clang-tidy.cmake)

if(LN_LITTLEFS)
  include(cmake/extern/littlefs.cmake)
endif()

if(LN_LUA)
  add_subdirectory(extern/lua)
endif()

# NOTE: ln library is composed of multiple ln::* modular targets, the ln is an
# umbrella target that includes all of them for convenience.
add_library(ln INTERFACE)
add_library(ln::ln ALIAS ln)

add_subdirectory(ln)

if(CROSSCOMPILE_TEST OR LN_FREERTOS)
  add_subdirectory(extern/FreeRTOS-Cpp)
  target_link_libraries(ln INTERFACE FreeRTOS-Cpp)
endif()
