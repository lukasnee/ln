add_library(fonas)

target_sources(
  fonas
  PRIVATE src/fonas.cpp
  src/EventDrivenSpi.cpp
          src/EventDrivenReader.cpp
          src/EventDrivenWriter.cpp
          src/EventDrivenReaderWriter.cpp
          src/StreamBuffer.cpp
)

target_include_directories(fonas PUBLIC include)

target_link_libraries(fonas PUBLIC freertos-addons)