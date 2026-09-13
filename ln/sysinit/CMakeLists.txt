add_library(ln_sysinit INTERFACE)
target_include_directories(ln_sysinit INTERFACE include)
target_link_libraries(ln_sysinit INTERFACE ln::core ln::algo)

add_library(ln::sysinit ALIAS ln_sysinit)
target_link_libraries(ln INTERFACE ln::sysinit)
