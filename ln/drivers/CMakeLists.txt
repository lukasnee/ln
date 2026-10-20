ln_add_library(ln_drivers ALIAS ln::drivers)
target_sources(ln_drivers PRIVATE src/EventDrivenSpi.cpp)
target_include_directories(ln_drivers PUBLIC include)
target_link_libraries(ln_drivers PUBLIC ln::core)
