cmake_minimum_required(VERSION 3.24)
project(usm_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(usm
  src/tools.cpp
  src/page.cpp
  src/chunk.cpp
  src/usm.cpp
  src/media.cpp
        include/usm/codecs/ivf_vp9.hpp
        include/usm/codecs/h264.hpp
        include/usm/codecs/hca.hpp
        include/usm/io.hpp
)

target_include_directories(usm PUBLIC include)

find_package(FFMPEG REQUIRED)
find_package(ICU REQUIRED COMPONENTS uc i18n)

message(STATUS "FFMPEG_INCLUDE_DIRS=${FFMPEG_INCLUDE_DIRS}")
message(STATUS "FFMPEG_LIBRARY_DIRS=${FFMPEG_LIBRARY_DIRS}")
message(STATUS "FFMPEG_LIBRARIES=${FFMPEG_LIBRARIES}")

# FindFFMPEG (as provided by vcpkg here) exposes variables, not targets.
target_include_directories(usm PUBLIC ${FFMPEG_INCLUDE_DIRS})
target_link_directories(usm PUBLIC ${FFMPEG_LIBRARY_DIRS})

target_link_libraries(usm
  PUBLIC
    ${FFMPEG_LIBRARIES}
    ICU::uc
    ICU::i18n
)

add_executable(usmtool apps/usmtool.cpp)
target_link_libraries(usmtool PRIVATE usm)