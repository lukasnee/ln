add_subdirectory(core)
add_subdirectory(logger)
if(LN_FREERTOS)
  add_subdirectory(drivers)
  add_subdirectory(shell) # TODO FreeRTOS should probably be decoupled from
                          # shell
endif()
add_subdirectory(syscalls)
