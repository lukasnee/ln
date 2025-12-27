add_library(ln_shell)
add_library(ln::shell ALIAS ln_shell)
target_sources(
  ln_shell PRIVATE src/Arg.cpp src/CLI.cpp src/Input.cpp src/Cmd.cpp
                   src/Parser.cpp src/cmds/generic/generic.cpp)
target_include_directories(ln_shell PUBLIC include)
target_link_libraries(ln_shell PUBLIC ln_core ln_logger FreeRTOS-Cpp)
target_link_libraries(ln INTERFACE ln::shell)

add_subdirectory(src/cmds/core) # TODO rename dir core -> general
