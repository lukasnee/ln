cmake_minimum_required(VERSION 3.25)

project(
  ln
  VERSION 0.1.0
  LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
  add_subdirectory(tests)
  set(FREERTOS_PORT
      GCC_ARM_CM7
      CACHE STRING \"\")
  set(FREERTOS_HEAP 4)
  add_library(freertos_config INTERFACE)
  target_link_libraries(freertos_config INTERFACE ln::tests) # FreeRTOSCOnfig.h
endif()

add_subdirectory(extern/FreeRTOS-Cpp)

add_subdirectory(ln)

add_library(ln INTERFACE)

target_link_libraries(ln INTERFACE ln::core ln::drivers ln::logger FreeRTOS-Cpp)

add_library(ln::ln ALIAS ln)
