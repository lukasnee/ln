include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.11.0)
FetchContent_MakeAvailable(Catch2)

include(CTest)
include(Catch)

add_executable(test_ringbuffer RingBufferTests.cpp)
target_link_libraries(test_ringbuffer PRIVATE Catch2::Catch2WithMain ln_core)
catch_discover_tests(test_ringbuffer)
