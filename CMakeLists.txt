cmake_minimum_required(VERSION 3.12)

project(HEVC_FLV_Remux)

find_package(Python3 REQUIRED COMPONENTS Development)
find_package(CURL REQUIRED)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/build/FFmpeg/lib/pkgconfig)
    MESSAGE(FATAL_ERROR "Embedded FFmpeg not built.\n"
                        "Please run the following script first:\n"
                        "  `${CMAKE_CURRENT_SOURCE_DIR}/build_ffmpeg.sh`")
endif()

find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/build/FFmpeg/lib/pkgconfig)
pkg_check_modules(FFmpeg REQUIRED libavutil libavformat libavcodec)

list(FIND FFmpeg_LDFLAGS -pthread PTHREAD_OPTION)
if (NOT ${PTHREAD_OPTION} EQUAL -1)
    list(APPEND FFmpeg_LINK_LIBRARIES -pthread)
endif()

SET(ENABLE_TARGET_EXPORT OFF)
SET(CJSON_BUILD_SHARED_LIBS OFF)
SET(CJSON_OVERRIDE_BUILD_SHARED_LIBS ON)
add_subdirectory(cJSON EXCLUDE_FROM_ALL)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_library(remux STATIC src/remux.c)
add_library(remuxmodule MODULE src/remuxmodule.c)
add_executable(remuxing src/remuxing.c)
add_executable(bili-live src/bili-live.c)
add_executable(flv_checker src/flv_checker.c)

target_include_directories(remux PUBLIC src ${FFmpeg_INCLUDE_DIRS})
target_link_libraries(remux PUBLIC ${FFmpeg_LINK_LIBRARIES})

target_include_directories(remuxing PUBLIC src ${FFmpeg_INCLUDE_DIRS})
target_link_libraries(remuxing PUBLIC remux ${FFmpeg_LINK_LIBRARIES})

target_include_directories(bili-live PUBLIC src CURL::libcurl cJSON ${FFmpeg_INCLUDE_DIRS})
target_link_libraries(bili-live PUBLIC CURL::libcurl cjson remux ${FFmpeg_LINK_LIBRARIES})

target_include_directories(remuxmodule PUBLIC src ${Python3_INCLUDE_DIRS} ${FFmpeg_INCLUDE_DIRS})
target_link_libraries(remuxmodule PUBLIC ${Python3_LIBRARIES} ${FFmpeg_LINK_LIBRARIES} remux)
set_target_properties(remuxmodule PROPERTIES OUTPUT_NAME remux PREFIX "" SUFFIX .so)

if(CMAKE_BUILD_TYPE STREQUAL Release)
    add_custom_command(TARGET remuxing POST_BUILD COMMAND ${CMAKE_STRIP} remuxing)
    add_custom_command(TARGET bili-live POST_BUILD COMMAND ${CMAKE_STRIP} bili-live)
    add_custom_command(TARGET flv_checker POST_BUILD COMMAND ${CMAKE_STRIP} flv_checker)
    add_custom_command(TARGET remuxmodule POST_BUILD COMMAND ${CMAKE_STRIP} -S remux.so)
endif()

option(INSTALL_TO_USER_DIR "Install Python module to user site" ON)

if(INSTALL_TO_USER_DIR)
    execute_process(COMMAND python3 -m site --user-site
                    OUTPUT_VARIABLE PYTHON_SITE_PATH
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
else()
    string(APPEND PYTHON_SITE_PATH
                  ${CMAKE_INSTALL_PREFIX}
                  "/lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}"
                  "/site-packages")
endif()

install(TARGETS remuxmodule DESTINATION ${PYTHON_SITE_PATH})
install(TARGETS remuxing)
install(TARGETS bili-live)
