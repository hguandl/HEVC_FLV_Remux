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

if(NOT EXISTS ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
    MESSAGE(FATAL_ERROR "Embedded FFmpeg not built.\n"
                        "Please run the following script first:\n"
                        "  `${CMAKE_CURRENT_SOURCE_DIR}/build_ffmpeg.sh`")
endif()

find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH} ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
pkg_check_modules(FFmpeg REQUIRED libavutil libavformat libavcodec)

add_library(remux remux.c)
add_executable(remuxing remuxing.c)
add_library(remuxmodule SHARED remuxmodule.c)

target_include_directories(remux PUBLIC ${CMAKE_INSTALL_PREFIX}/include)
target_link_libraries(remux PUBLIC ${FFmpeg_LINK_LIBRARIES})

target_include_directories(remuxing PUBLIC ${CMAKE_INSTALL_PREFIX}/include)
target_link_libraries(remuxing PUBLIC ${FFmpeg_LINK_LIBRARIES} remux)

target_include_directories(remuxmodule PUBLIC ${Python3_INCLUDE_DIRS} ${CMAKE_INSTALL_PREFIX}/include)
target_link_libraries(remuxmodule PUBLIC ${FFmpeg_LINK_LIBRARIES} ${Python3_LIBRARIES} remux)
set_target_properties(remuxmodule PROPERTIES OUTPUT_NAME remux PREFIX "" SUFFIX .so)

if(CMAKE_BUILD_TYPE STREQUAL Release)
    add_custom_command(TARGET remuxing POST_BUILD COMMAND ${CMAKE_STRIP} remuxing)
endif()

install(TARGETS remuxmodule DESTINATION .)
