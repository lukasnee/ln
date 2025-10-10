add_library(ln_shell)
add_library(ln::shell ALIAS ln_shell)
target_sources(ln_shell PRIVATE src/shell.cpp src/input.cpp src/commands.cpp
                                src/argVector.cpp src/argBuffer.cpp)
target_include_directories(ln_shell PUBLIC include)
target_link_libraries(ln_shell PUBLIC ln::core)
