add_library(ln_port_FreeRTOS)
add_library(ln::port::FreeRTOS ALIAS ln_port_FreeRTOS)

target_sources(ln_port_FreeRTOS PRIVATE src/port.cpp src/task_registry.cpp)
target_include_directories(ln_port_FreeRTOS PUBLIC include)
target_link_libraries(ln_port_FreeRTOS PUBLIC ln::core FreeRTOS-Cpp)

if(TARGET freertos_config)
  target_include_directories(freertos_config INTERFACE include)
  if(LN_PROF)
    target_compile_definitions(freertos_config INTERFACE LN_PROF)
  endif()
endif()

add_library(ln_port_FreeRTOS_shell_cmds INTERFACE)
add_library(ln::port::FreeRTOS::shell_cmds ALIAS ln_port_FreeRTOS_shell_cmds)

target_link_libraries(ln_port_FreeRTOS_shell_cmds INTERFACE ln::port::FreeRTOS
                                                            ln::shell)
target_sources(ln_port_FreeRTOS_shell_cmds INTERFACE src/port_shell_cmds.cpp)
