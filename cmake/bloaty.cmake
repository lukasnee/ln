include_guard()

option(LN_BLOATY "Enable Bloaty size profiler tool" OFF)

if(NOT LN_BLOATY)
  return()
endif()

find_program(BLOATY_EXECUTABLE bloaty)
if(NOT BLOATY_EXECUTABLE)
  message(
    FATAL_ERROR
      "Bloaty executable not found on system. Either install bloaty or disable LN_BLOATY"
  )
endif()

# Generate a bloaty report comparing the current build output to the previous
# build output (if it exists). This allows tracking what source files and
# symbols are contributing to changes in the firmware size across builds.
function(ln_bloaty_compare_with_previous_build fw_target)
  if(EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${fw_target}.bloaty.old)
    add_custom_command(
      TARGET ${fw_target}
      POST_BUILD
      COMMAND echo
              "Bloaty report comparing ${fw_target} to previous build output:"
      COMMAND
        ${BLOATY_EXECUTABLE} --domain=vm -d compileunits,symbols
        ${CMAKE_CURRENT_BINARY_DIR}/${fw_target} --
        ${CMAKE_CURRENT_BINARY_DIR}/${fw_target}.bloaty.old
      COMMAND
        echo
        "Generating bloaty report comparing ${fw_target} to previous build output..."
    )
  else()
    add_custom_command(
      TARGET ${fw_target}
      POST_BUILD
      COMMAND
        echo
        "No previous build output found for ${fw_target}, skipping bloaty report generation..."
    )
  endif()

  add_custom_command(
    TARGET ${fw_target}
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_CURRENT_BINARY_DIR}/${fw_target}
      ${CMAKE_CURRENT_BINARY_DIR}/${fw_target}.bloaty.old
    COMMAND echo "Previous build output copied to ${fw_target}.bloaty.old")
endfunction()
