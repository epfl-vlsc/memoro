cmake_minimum_required(VERSION 3.12)
project(memoro-stats VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(memoro
  Memoro.hpp
  Trace.cpp
  TraceReader.hpp TraceReader.cpp
  ChunkReader.hpp ChunkReader.cpp
  TraceStatReader.hpp
  TraceStatWriter.hpp TraceStatWriter.cpp
)

add_executable(memoro-stats Main.cpp)
target_link_libraries(memoro-stats PRIVATE memoro)
