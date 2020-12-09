#!/bin/sh
set -e

curl -L -o- https://github.com/ksvc/FFmpeg/archive/release/3.4.tar.gz | \
    tar zxf -

pushd FFmpeg-release-3.4
./configure \
    --prefix=$PWD/../ffmpeg \
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
    --enable-protocol=file

make -j
make install
popd
