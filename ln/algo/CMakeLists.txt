ln_add_library(ln_algo ALIAS ln::algo INTERFACE)
target_include_directories(ln_algo INTERFACE include)
target_link_libraries(ln_algo INTERFACE ln::core)

if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
  add_subdirectory(tests)
endif()
