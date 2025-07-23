add_library(ln_core)
add_library(ln::core ALIAS ln_core)
target_sources(ln_core PRIVATE src/ln.cpp)
target_include_directories(ln_core PUBLIC include)
