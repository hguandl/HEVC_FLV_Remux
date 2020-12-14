#!/usr/bin/env bash
set -e

pushd `dirname $0`
PROJECT_ROOT=`pwd`
popd

if [ ! -d $PROJECT_ROOT/FFmpeg-stage ]; then
    mkdir -p $PROJECT_ROOT/FFmpeg-stage
fi

pushd $PROJECT_ROOT/FFmpeg-stage
$PROJECT_ROOT/FFmpeg/configure \
                     --prefix=$PROJECT_ROOT/FFmpeg-build \
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
                     --disable-asm

make -j
make install
popd
