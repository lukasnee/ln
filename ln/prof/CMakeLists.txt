set(LN_PROF_CONFIG_DIR
    ""
    CACHE
      PATH
      "Directory where user's profiler configuration file (prof_config.h) is located"
)

option(LN_PROF_STATS "Collect ln::prof overhead statistics (adds overhead)" OFF)

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
  if(NOT EXISTS "${LN_PROF_CONFIG_DIR}/prof_config.h")
    message(
      FATAL_ERROR
        "ln::prof requires prof_config.h, make sure LN_PROF_CONFIG_DIR is set correctly"
    )
  endif()
  ln_add_library(ln_prof ALIAS ln::prof)
  target_link_libraries(ln_prof PRIVATE ln::core)
  if(LN_FREERTOS)
    target_link_libraries(ln_prof PRIVATE FreeRTOS-Cpp)
    target_compile_definitions(ln_prof PRIVATE LN_PROF_RTOS)
  endif()
  target_compile_options(ln_prof PRIVATE -O3)
  target_compile_definitions(ln_prof PUBLIC LN_PROF)
  if(LN_PROF_STATS)
    target_compile_definitions(ln_prof PRIVATE LN_PROF_STATS)
    target_link_libraries(ln_prof PRIVATE ln::shell)
  endif()
  target_include_directories(ln_prof PUBLIC include ${LN_PROF_CONFIG_DIR})
  target_sources(ln_prof PRIVATE src/prof.cpp)
  set_source_files_properties(src/prof.cpp PROPERTIES COMPILE_OPTIONS
                                                      -fno-instrument-functions)
endif()
