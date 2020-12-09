cmake_minimum_required(VERSION 3.10)

project(HEVC_FLV_Remux)
include(FindPkgConfig)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg/lib/pkgconfig)
    MESSAGE(FATAL_ERROR "Embedded FFmpeg not built.\n"
                        "Please run the following script first:\n"
                        "  `${CMAKE_CURRENT_SOURCE_DIR}/build_ffmpeg.sh`")
endif()

set(ENV{PKG_CONFIG_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg/lib/pkgconfig)
pkg_check_modules(libffmpeg REQUIRED libavutil libavformat libavcodec)

add_executable(remuxing remuxing.c)
target_include_directories(remuxing PUBLIC ${libffmpeg_INCLUDE_DIRS})
target_link_directories(remuxing PUBLIC ${libffmpeg_LIBRARY_DIRS})
target_link_libraries(remuxing PUBLIC ${libffmpeg_LINK_LIBRARIES})
