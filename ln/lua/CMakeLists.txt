ln_add_library(ln_lua ALIAS ln::lua)
target_link_libraries(ln_lua PUBLIC lua ln::core)
target_sources(ln_lua PRIVATE src/VM.cpp)
target_include_directories(ln_lua PUBLIC include)
