find_program(CLANG_TIDY_EXE "clang-tidy")

if(CLANG_TIDY_EXE)
  message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
else()
  message(WARNING "clang-tidy not found!")
endif()

if(CLANG_TIDY_EXE AND CMAKE_CROSSCOMPILING)
  # When cross-compiling, clang-tidy needs manual help with the target triple
  # and the include paths to find the correct standard library headers.
  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR})
  list(APPEND CMAKE_CXX_CLANG_TIDY --header-filter=${CMAKE_SOURCE_DIR}/ln/.*)
  if(CMAKE_CXX_CLANG_TIDY AND CMAKE_CXX_COMPILER MATCHES .*arm-none-eabi.*)
    list(APPEND CMAKE_CXX_CLANG_TIDY --extra-arg=--target=arm-none-eabi)
    set(implicit_includes ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES}
                          ${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES})
    list(REMOVE_DUPLICATES implicit_includes)
    foreach(include ${implicit_includes})
      # Skip GCC's builtin headers because clang does not support the same
      # builtins and intrinsics. Clang provides its own so-called resource
      # headers for that purpose, which come bundled with the compiler not the
      # standard library.
      if(${include} MATCHES .*lib/gcc.*)
        continue()
      endif()
      list(APPEND CMAKE_CXX_CLANG_TIDY "--extra-arg=-isystem${include}")
    endforeach()
  endif()
  message(
    VERBOSE
    "You may use the following configurations for .pre-commit-config.yaml clang-tidy hook args: ${CMAKE_CXX_CLANG_TIDY}"
  )
endif()
