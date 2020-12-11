cmake_minimum_required(VERSION 3.13)

project(HEVC_FLV_Remux)

find_package(Python3 REQUIRED COMPONENTS Development)

if(NOT DEFINED CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()

string(APPEND CMAKE_INSTALL_PREFIX
              "/lib"
              "/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}"
              "/site-packages"
              "/remux")

if(INSTALL_TO_USER_DIR)
    execute_process(COMMAND python3 -m site --user-site
                    OUTPUT_VARIABLE CMAKE_INSTALL_PREFIX
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(APPEND CMAKE_INSTALL_PREFIX /remux)
endif()

include(FetchContent)
FetchContent_Declare(
    FFmpegSrc
    URL https://github.com/ksvc/FFmpeg/archive/release/3.4.tar.gz
)
FetchContent_GetProperties(FFmpegSrc)
if(NOT FFmpegSrc_POPULATED)
    message(STATUS "Retrieving FFmpeg source code...")
    FetchContent_Populate(FFmpegSrc)
endif()

LIST(APPEND FFmpegLibs
            -L${CMAKE_INSTALL_PREFIX}/lib
            -lavutil
            -lavcodec
            -lavformat)

LIST(APPEND FFmpegConfArgs
            --prefix=${CMAKE_INSTALL_PREFIX}
            --disable-static
            --enable-shared
            --disable-programs
            --disable-doc
            --disable-avdevice
            --disable-swresample
            --disable-swscale
            --disable-avfilter
            --disable-everything
            --enable-decoder=aac,h264,hevc
            --enable-parser=aac,h264,hevc
            --enable-demuxer=aac,flv,live_flv,h264,hevc
            --enable-muxer=aac,flv,h264,hevc,mp4,m4v,mov
            --enable-protocol=file,http,httpproxy,https,tls_openssl
            --enable-openssl
            --disable-securetransport)

if(NOT EXISTS ${ffmpegsrc_SOURCE_DIR}/config.h)
    message(STATUS "Configuring FFmpeg...")
    execute_process(COMMAND ./configure ${FFmpegConfArgs}
                    WORKING_DIRECTORY ${ffmpegsrc_SOURCE_DIR})
endif()

include(ExternalProject)
ExternalProject_Add(
    FFmpeg
    SOURCE_DIR ${ffmpegsrc_SOURCE_DIR}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${MAKE}
    INSTALL_COMMAND ${MAKE}
)

add_library(remux SHARED remux.c)
add_executable(remuxing remuxing.c)
add_library(remuxmodule MODULE remuxmodule.c)
add_dependencies(remux FFmpeg)

target_include_directories(remux PUBLIC ${ffmpegsrc_SOURCE_DIR})
target_link_options(remux PUBLIC ${FFmpegLibs})

target_include_directories(remuxing PUBLIC ${ffmpegsrc_SOURCE_DIR})
target_link_libraries(remuxing PUBLIC remux)
target_link_options(remuxing PUBLIC ${FFmpegLibs})

target_include_directories(remuxmodule PUBLIC ${Python3_INCLUDE_DIRS} ${ffmpegsrc_SOURCE_DIR})
target_link_libraries(remuxmodule PUBLIC ${Python3_LIBRARIES} remux)
target_link_options(remuxmodule PUBLIC ${FFmpegLibs})
set_target_properties(remuxmodule PROPERTIES OUTPUT_NAME remux PREFIX "" SUFFIX .so)

if(CMAKE_BUILD_TYPE STREQUAL Release)
    add_custom_command(TARGET remuxing POST_BUILD COMMAND ${CMAKE_STRIP} remuxing)
endif()

install(TARGETS remuxmodule DESTINATION .)
