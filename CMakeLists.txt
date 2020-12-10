cmake_minimum_required(VERSION 3.12)

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

list(APPEND FFMPEG_CONF_ARGS
    "--prefix=${CMAKE_INSTALL_PREFIX}"
    "--disable-static"
    "--enable-shared"
    "--disable-programs"
    "--disable-doc"
    "--disable-avdevice"
    "--disable-swresample"
    "--disable-swscale"
    "--disable-avfilter"
    "--disable-everything"
    "--enable-decoder=aac,h264,hevc"
    "--enable-parser=aac,h264,hevc"
    "--enable-demuxer=aac,flv,live_flv,h264,hevc"
    "--enable-muxer=aac,flv,h264,hevc,mp4,m4v,mov"
    "--enable-protocol=file,http,httpproxy,https,tls_openssl"
    "--enable-openssl"
    "--disable-securetransport")

include(ExternalProject)

ExternalProject_Add(
    FFmpeg
    URL "https://github.com/ksvc/FFmpeg/archive/release/3.4.tar.gz"
    CONFIGURE_COMMAND <SOURCE_DIR>/configure ${FFMPEG_CONF_ARGS}
    BUILD_COMMAND ${MAKE}
    INSTALL_COMMAND ${MAKE}
)

if(UNIX)
    if(APPLE)
        set(LIB_SUFFIX .dylib)
    else()
        set(LIB_SUFFIX .so)
    endif()
endif

list(APPEND FFmpeg_LIBS
            ${CMAKE_INSTALL_PREFIX}/lib/libavutil.${LIB_SUFFIX}
            ${CMAKE_INSTALL_PREFIX}/lib/libavformat.${LIB_SUFFIX}
            ${CMAKE_INSTALL_PREFIX}/lib/libavcodec.${LIB_SUFFIX})

add_library(remux remux.c)
add_executable(remuxing remuxing.c)
add_library(remuxmodule SHARED remuxmodule.c)
add_dependencies(remux FFmpeg)

target_include_directories(remux PUBLIC ${CMAKE_INSTALL_PREFIX}/include)
target_link_libraries(remux PUBLIC ${FFmpeg_LIBS})

target_include_directories(remuxing PUBLIC ${CMAKE_INSTALL_PREFIX}/include)
target_link_libraries(remuxing PUBLIC ${FFmpeg_LIBS} remux)

target_include_directories(remuxmodule PUBLIC ${Python3_INCLUDE_DIRS} ${CMAKE_INSTALL_PREFIX}/include)
target_link_libraries(remuxmodule PUBLIC ${FFmpeg_LIBS} ${Python3_LIBRARIES} remux)
set_target_properties(remuxmodule PROPERTIES OUTPUT_NAME remux PREFIX "" SUFFIX .so)

if(CMAKE_BUILD_TYPE STREQUAL Release)
    add_custom_command(TARGET remuxing POST_BUILD COMMAND ${CMAKE_STRIP} remuxing)
endif()

install(TARGETS remuxmodule DESTINATION .)
