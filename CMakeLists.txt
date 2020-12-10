cmake_minimum_required(VERSION 3.10)

project(HEVC_FLV_Remux)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg/lib/pkgconfig)
    MESSAGE(FATAL_ERROR "Embedded FFmpeg not built.\n"
                        "Please run the following script first:\n"
                        "  `${CMAKE_CURRENT_SOURCE_DIR}/build_ffmpeg.sh`")
endif()

find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg/lib/pkgconfig)
pkg_check_modules(libffmpeg REQUIRED libavutil libavformat libavcodec)

list(FIND libffmpeg_LDFLAGS -pthread index)
if(NOT ${index} EQUAL -1)
    list(APPEND extra_LIBRARIES pthread)
endif()

add_library(remux remux.c)
add_executable(remuxing remuxing.c)

target_include_directories(remux PUBLIC ${libffmpeg_INCLUDE_DIRS})
target_link_directories(remux PUBLIC ${libffmpeg_LIBRARY_DIRS})
target_link_libraries(remux PUBLIC ${libffmpeg_LINK_LIBRARIES} ${extra_LIBRARIES})

target_include_directories(remuxing PUBLIC ${libffmpeg_INCLUDE_DIRS})
target_link_directories(remuxing PUBLIC ${libffmpeg_LIBRARY_DIRS})
target_link_libraries(remuxing PUBLIC ${libffmpeg_LINK_LIBRARIES} ${extra_LIBRARIES} remux)

if (CMAKE_BUILD_TYPE STREQUAL Release)
    add_custom_command(TARGET remuxing POST_BUILD COMMAND ${CMAKE_STRIP} remuxing)
endif()
