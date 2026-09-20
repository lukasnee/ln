ln_enable_clang_tidy()

add_subdirectory(stdcpp)
if(LN_FREERTOS)
  add_subdirectory(FreeRTOS)
  if(NOT PROJECT_IS_TOP_LEVEL)
    add_subdirectory(STM32F3_FreeRTOS)
  endif()
endif()
