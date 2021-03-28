#!/usr/bin/env bash
set -e

FFMPEG_VERSION="4.3.2"
FFMPEG_URL="https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz"

pushd `dirname $0`
PROJECT_ROOT=`pwd`

if [ ! -d "ffmpeg-${FFMPEG_VERSION}" ]; then
    if [ ! -f "ffmpeg-${FFMPEG_VERSION}.tar.xz" ]; then
        echo "Retriving FFmpeg source code..."
        curl -fsLJOS ${FFMPEG_URL}
    fi

    echo "Unpacking FFmpeg..."
    tar xf ffmpeg-${FFMPEG_VERSION}.tar.xz

    echo "Patching FFmpeg..."
    pushd $PROJECT_ROOT/ffmpeg-${FFMPEG_VERSION}
    patch -p01 -i $PROJECT_ROOT/patches/ffmpeg-4.x-hevc-flv.patch
    popd
fi
popd

if [ ! -d $PROJECT_ROOT/build/FFmpeg/BuildFiles ]; then
    mkdir -p $PROJECT_ROOT/build/FFmpeg/BuildFiles
fi

echo "Configuring FFmpeg..."
pushd $PROJECT_ROOT/build/FFmpeg/BuildFiles
$PROJECT_ROOT/ffmpeg-${FFMPEG_VERSION}/configure \
                     --prefix=$PROJECT_ROOT/build/FFmpeg \
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
                     --disable-securetransport \
                     --disable-asm \
                     --extra-cflags=-fPIC

echo "Building FFmpeg..."
make -j
make install
popd
