cmake_minimum_required(VERSION 3.12)

project(HEVC_FLV_Remux)

find_package(Python3 REQUIRED COMPONENTS Development)
find_package(CURL REQUIRED)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/FFmpeg-build/lib/pkgconfig)
    MESSAGE(FATAL_ERROR "Embedded FFmpeg not built.\n"
                        "Please run the following script first:\n"
                        "  `${CMAKE_CURRENT_SOURCE_DIR}/build_ffmpeg.sh`")
endif()

find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/FFmpeg-build/lib/pkgconfig)
pkg_check_modules(FFmpeg REQUIRED libavutil libavformat libavcodec)

list(FIND FFmpeg -pthread PTHREAD_OPTION)
if (NOT ${PTHREAD_OPTION} EQUAL -1)
    list(APPEND FFmpeg_LINK_LIBRARIES -pthread)
endif()

SET(ENABLE_TARGET_EXPORT OFF)
SET(CJSON_BUILD_SHARED_LIBS OFF)
SET(CJSON_OVERRIDE_BUILD_SHARED_LIBS ON)
add_subdirectory(cJSON EXCLUDE_FROM_ALL)

add_library(remux STATIC remux.c)
add_executable(remuxing remuxing.c)
add_library(remuxmodule MODULE remuxmodule.c)
add_executable(bili-live bili-live.c)
add_executable(flv_checker flv_checker.c)

target_include_directories(remux PUBLIC ${FFmpeg_INCLUDE_DIRS})
target_link_libraries(remux PUBLIC ${FFmpeg_LINK_LIBRARIES})

target_include_directories(remuxing PUBLIC ${FFmpeg_INCLUDE_DIRS})
target_link_libraries(remuxing PUBLIC remux ${FFmpeg_LINK_LIBRARIES})

target_include_directories(bili-live PUBLIC CURL::libcurl cJSON ${FFmpeg_INCLUDE_DIRS})
target_link_directories(bili-live PUBLIC ${FFmpeg_LIBRARY_DIRS})
target_link_libraries(bili-live PUBLIC CURL::libcurl cjson remux)

target_include_directories(remuxmodule PUBLIC ${Python3_INCLUDE_DIRS} ${FFmpeg_INCLUDE_DIRS})
target_link_libraries(remuxmodule PUBLIC ${Python3_LIBRARIES} ${FFmpeg_LINK_LIBRARIES} remux)
set_target_properties(remuxmodule PROPERTIES OUTPUT_NAME remux PREFIX "" SUFFIX .so)

if(CMAKE_BUILD_TYPE STREQUAL Release)
    add_custom_command(TARGET remuxing POST_BUILD COMMAND ${CMAKE_STRIP} remuxing)
    add_custom_command(TARGET bili-live POST_BUILD COMMAND ${CMAKE_STRIP} bili-live)
endif()

if(INSTALL_TO_USER_DIR)
    execute_process(COMMAND python3 -m site --user-site
                    OUTPUT_VARIABLE PYTHON_SITE_PATH
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(APPEND PYTHON_SITE_PATH /remux)
else()
    string(APPEND PYTHON_SITE_PATH
                  ${CMAKE_INSTALL_PREFIX}
                  "/lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}"
                  "/site-packages"
                  "/remux")
endif()

install(TARGETS remuxmodule DESTINATION ${PYTHON_SITE_PATH})
