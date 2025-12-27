add_library(ln_core INTERFACE)
add_library(ln::core ALIAS ln_core)
target_include_directories(ln_core INTERFACE include)
target_link_libraries(ln INTERFACE ln::core)
