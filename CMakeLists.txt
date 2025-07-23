add_library(fonas)

target_sources(fonas PRIVATE src/fonas.cpp src/EventDrivenSpi.cpp
                             src/StreamBuffer.cpp src/logger/logger.cpp)

target_include_directories(fonas PUBLIC include)

target_link_libraries(fonas PUBLIC freertos-addons)
