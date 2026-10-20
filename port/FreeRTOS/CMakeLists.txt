add_library(ln_port_FreeRTOS)
add_library(ln::port::FreeRTOS ALIAS ln_port_FreeRTOS)

target_sources(ln_port_FreeRTOS PRIVATE src/port.cpp)
target_include_directories(ln_port_FreeRTOS PUBLIC include)
target_link_libraries(ln_port_FreeRTOS PUBLIC ln::core FreeRTOS-Cpp)

if(LN_PROF AND TARGET freertos_config)
  target_compile_definitions(freertos_config INTERFACE LN_PROF)
  target_include_directories(freertos_config INTERFACE include)
endif()

add_library(ln_port_FreeRTOS_shell_cmds INTERFACE)
add_library(ln::port::FreeRTOS::shell_cmds ALIAS ln_port_FreeRTOS_shell_cmds)

target_link_libraries(ln_port_FreeRTOS_shell_cmds INTERFACE ln::port::FreeRTOS
                                                            ln::shell)
target_sources(ln_port_FreeRTOS_shell_cmds INTERFACE src/port_shell_cmds.cpp)
