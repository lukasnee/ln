if(NOT LN_FREERTOS)
  # ln::prof could be made to work without an RTOS, but currently it is only
  # implemented for FreeRTOS for simplicity sake.
  message(FATAL_ERROR "ln_prof requires FreeRTOS")
endif()

set(LN_PROF_ITM_BASE_PORT
    0
    CACHE STRING "The ITM base port where to map ln::prof protocol ports")

set(LN_PROF_FW_SIZE
    0x80000
    CACHE STRING "Bytes of code from the start of flash ln::prof can profile")

option(LN_PROF_STATS "Collect ln::prof overhead statistics (adds overhead)" OFF)

set(LN_PROF_HOT_SECTION
    ""
    CACHE STRING "Linker section for the ln::prof hot path, e.g. .ccmram")

function(ln_prof)

  if(NOT LN_PROF)
    return()
  endif()

  set(options "")
  set(oneValueArgs TARGET TARGET_SCOPE)
  set(multiValueArgs EXCLUDE_FUNCTIONS EXCLUDE_FILES)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}"
                        ${ARGN})

  if(ARG_EXCLUDE_FUNCTIONS)
    string(REPLACE ";" "," exclude_functions_val "${ARG_EXCLUDE_FUNCTIONS}")
    set(exclude_functions_opt
        "-finstrument-functions-exclude-function-list=${exclude_functions_val}")
  endif()

  if(ARG_EXCLUDE_FILES)
    string(REPLACE ";" "," exclude_files_val "${ARG_EXCLUDE_FILES}")
    set(exclude_files_opt
        "-finstrument-functions-exclude-file-list=${exclude_files_val}")
  endif()

  if(ARG_TARGET)
    if(NOT ARG_TARGET_SCOPE)
      set(ARG_TARGET_SCOPE PRIVATE)
    endif()
    target_compile_options(
      ${ARG_TARGET} ${ARG_TARGET_SCOPE} -finstrument-functions
      ${exclude_functions_opt} ${exclude_files_opt})
  else()
    add_compile_options(-finstrument-functions ${exclude_functions_opt}
                        ${exclude_files_opt})
  endif()
endfunction()

if(NOT LN_PROF)
  ln_add_library(ln_prof ALIAS ln::prof INTERFACE)
  target_include_directories(ln_prof INTERFACE include)
else()
  ln_add_library(ln_prof ALIAS ln::prof)
  target_link_libraries(ln_prof PRIVATE ln::core FreeRTOS-Cpp)
  target_compile_options(ln_prof PRIVATE -O3)
  target_compile_definitions(
    ln_prof
    PUBLIC LN_PROF
    PRIVATE LN_PROF_ITM_BASE_PORT=${LN_PROF_ITM_BASE_PORT}
            LN_PROF_FW_SIZE=${LN_PROF_FW_SIZE})
  if(LN_PROF_STATS)
    target_compile_definitions(ln_prof PRIVATE LN_PROF_STATS)
    target_link_libraries(ln_prof PRIVATE ln::shell)
  endif()
  if(LN_PROF_HOT_SECTION)
    target_compile_definitions(
      ln_prof PRIVATE "LN_PROF_HOT_SECTION=\"${LN_PROF_HOT_SECTION}\"")
  endif()
  target_include_directories(ln_prof PUBLIC include)
  target_sources(ln_prof PRIVATE src/prof.cpp)
  set_source_files_properties(src/prof.cpp PROPERTIES COMPILE_OPTIONS
                                                      -fno-instrument-functions)
endif()
