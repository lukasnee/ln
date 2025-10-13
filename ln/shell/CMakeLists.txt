add_library(ln_shell)
add_library(ln::shell ALIAS ln_shell)
target_sources(
  ln_shell PRIVATE src/CLI.cpp src/Input.cpp src/Cmd.cpp src/ArgVector.cpp
                   src/ArgBuffer.cpp src/cmds/generic/generic.cpp)
target_include_directories(ln_shell PUBLIC include)
target_link_libraries(ln_shell PUBLIC ln::core)

add_subdirectory(src/cmds/core)
