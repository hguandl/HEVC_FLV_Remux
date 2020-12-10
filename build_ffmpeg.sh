#!/usr/bin/env bash
set -e

if [[ -f ffmpeg/lib/pkgconfig ]]; then
    echo "FFmpeg already installed"
    exit
fi

if [[ ! -d FFmpeg-release-3.4 ]]; then
    curl -L -o- https://github.com/ksvc/FFmpeg/archive/release/3.4.tar.gz | \
        tar zxf -
fi

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
    --enable-protocol=file,http,httpproxy,https,tls_openssl \
    --enable-openssl

make -j
make install
popd
