ln_add_library(ln_sysinit ALIAS ln::sysinit INTERFACE)
target_include_directories(ln_sysinit INTERFACE include)
target_link_libraries(ln_sysinit INTERFACE ln::core ln::algo)
