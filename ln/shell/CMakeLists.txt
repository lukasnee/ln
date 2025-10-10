add_library(ln_shell)
add_library(ln::shell ALIAS ln_shell)
target_sources(
  ln_shell PRIVATE src/Shell.cpp src/Input.cpp src/Command.cpp
                   src/ArgVector.cpp src/ArgBuffer.cpp src/generic/commands.cpp)
target_include_directories(ln_shell PUBLIC include)
target_link_libraries(ln_shell PUBLIC ln::core)
