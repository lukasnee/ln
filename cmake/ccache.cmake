include_guard()

# Macro, so the launcher variables are set in the caller's scope.
macro(ln_ccache_enable)
  find_program(CCACHE_PROGRAM ccache)
  if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    message(STATUS "ccache found: ${CCACHE_PROGRAM}")
  else()
    message(STATUS "ccache not found, building without it")
  endif()
endmacro()
