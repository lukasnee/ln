add_library(ln_tests INTERFACE)
add_library(ln::tests ALIAS ln_tests)
target_include_directories(ln_tests INTERFACE ${CMAKE_CURRENT_LIST_DIR}/config/include)