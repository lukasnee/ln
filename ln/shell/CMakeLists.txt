ln_add_library(ln_shell ALIAS ln::shell)
target_link_libraries(ln_shell PUBLIC ln::core)
target_sources(
  ln_shell
  PRIVATE src/Arg.cpp
          src/CLI.cpp
          src/Cmd.cpp
          src/History.cpp
          src/Buffer.cpp
          src/Parser.cpp
          src/cmds/generic/generic.cpp)
target_include_directories(ln_shell PUBLIC include)
add_subdirectory(src/cmds/core) # TODO rename dir core -> general
