#!/usr/bin/env bash
set -e

if [ -z $INSTALL_PREFIX ]; then
    INSTALL_PREFIX="/usr/local"
fi

FFMPEG_PREFIX="${INSTALL_PREFIX}/lib/python$(pkg-config --modversion python3)/site-packages/remux"

if [ -f ${FFMPEG_PREFIX}/lib/pkgconfig ]; then
    echo "FFmpeg already installed"
    exit
fi

if [ ! -d FFmpeg-release-3.4 ]; then

    if [ ! -f FFmpeg-release-3.4.tar.gz ]; then
        curl -L -oFFmpeg-release-3.4.tar.gz https://github.com/ksvc/FFmpeg/archive/release/3.4.tar.gz
    fi

    tar zxf FFmpeg-release-3.4.tar.gz
fi

pushd FFmpeg-release-3.4
./configure \
    --prefix="${FFMPEG_PREFIX}" \
    --disable-static \
    --enable-shared \
    --disable-programs \
    --disable-doc \
    --disable-avdevice \
    --disable-swresample \
    --disable-swscale \
    --disable-avfilter \
    --disable-everything \
    --enable-decoder=aac,h264,hevc \
    --enable-parser=aac,h264,hevc \
    --enable-demuxer=aac,flv,live_flv,h264,hevc \
    --enable-muxer=aac,flv,h264,hevc,mp4,m4v,mov \
    --enable-protocol=file,http,httpproxy,https,tls_openssl \
    --enable-openssl \
    --disable-securetransport

make -j
make install
popd
