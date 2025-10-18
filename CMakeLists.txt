cmake_minimum_required(VERSION 3.25)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LN_FREERTOS "Enable FreeRTOS and features depending on it" OFF)

project(
  ln
  VERSION 0.1.0
  LANGUAGES C CXX)

if(PROJECT_IS_TOP_LEVEL)
  add_subdirectory(tests)
  set(LN_FREERTOS ON)
  set(FREERTOS_PORT
      GCC_ARM_CM7
      CACHE STRING \"\")
  set(FREERTOS_HEAP 4)
  add_library(freertos_config INTERFACE)
  target_link_libraries(freertos_config INTERFACE ln::tests) # FreeRTOSCOnfig.h
endif()

include(cmake/extern/littlefs.cmake)

add_subdirectory(ln)

add_library(ln INTERFACE)

target_link_libraries(ln INTERFACE ln::core ln::drivers ln::logger ln::shell)

add_library(ln::ln ALIAS ln)

if(PROJECT_IS_TOP_LEVEL OR LN_FREERTOS)
  add_subdirectory(extern/FreeRTOS-Cpp)
  target_link_libraries(ln INTERFACE FreeRTOS-Cpp)
endif()
