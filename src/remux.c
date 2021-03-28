/*
 * Copyright (c) 2013 Stefano Sabatini
 *               2020 Hao Guan
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * Implementation of libremux.
 * Use forked FFmpeg by ksvc to handle unofficial FLV with HEVC stream.
 */

#include <signal.h>

#include <libavutil/log.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>

#include "libavutil/dict.h"
#include "libavutil/error.h"
#include "remux.h"

static int keyboard_interrupt = 0;

void handle_stop(int sig) {
    if (sig == SIGUSR1) {
        keyboard_interrupt = 1;
    }
}

int remux(const char *in_filename, const char *out_filename, const char *http_headers)
{
    AVOutputFormat *ofmt = NULL;
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    int ret, i;
    int stream_index = 0;
    int *stream_mapping = NULL;
    int stream_mapping_size = 0;
    int64_t *in_last_dts = NULL;
    int64_t *out_last_dts = NULL;
    AVDictionary *options = NULL;

    avformat_network_init();
    av_log_set_level(AV_LOG_WARNING);

    if (http_headers) {
        av_dict_set(&options, "timeout", "5000000", AV_DICT_APPEND);
        av_dict_set(&options, "headers", http_headers, AV_DICT_APPEND);
        av_dict_set(&options, "multiple_requests", "1", AV_DICT_APPEND);
        av_dict_set(&options, "reconnect_at_eof", "1", AV_DICT_APPEND);
        av_dict_set(&options, "reconnect_streamed", "1", AV_DICT_APPEND);
        av_dict_set(&options, "reconnect_delay_max", "3", AV_DICT_APPEND);
    }

    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, &options)) < 0) {
        fprintf(stderr, "Could not open input file '%s'\n", in_filename);
        goto end;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        fprintf(stderr, "Failed to retrieve input stream information\n");
        goto end;
    }

    av_dump_format(ifmt_ctx, 0, in_filename, 0);

    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    if (!ofmt_ctx) {
        fprintf(stderr, "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    stream_mapping_size = ifmt_ctx->nb_streams;
    stream_mapping = av_mallocz_array(stream_mapping_size, sizeof(*stream_mapping));
    if (!stream_mapping) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    in_last_dts = av_mallocz_array(stream_mapping_size, sizeof(int64_t));
    if (!in_last_dts) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    out_last_dts = av_mallocz_array(stream_mapping_size, sizeof(int64_t));
    if (!out_last_dts) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    ofmt = ofmt_ctx->oformat;

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *out_stream;
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVCodecParameters *in_codecpar = in_stream->codecpar;

        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            stream_mapping[i] = -1;
            continue;
        }

        stream_mapping[i] = stream_index++;

        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            fprintf(stderr, "Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
        if (ret < 0) {
            fprintf(stderr, "Failed to copy codec parameters\n");
            goto end;
        }

        if (out_stream->codecpar->codec_id == AV_CODEC_ID_HEVC) {
            out_stream->codecpar->codec_tag = MKTAG('h', 'v', 'c', '1');
        } else {
            out_stream->codecpar->codec_tag = 0;
        }

        in_last_dts[i] = AV_NOPTS_VALUE;
    }
    av_dump_format(ofmt_ctx, 0, out_filename, 1);

    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open output file '%s'", out_filename);
            goto end;
        }
    }

    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        goto end;
    }

    (void)signal(SIGUSR1, handle_stop);

    while (!keyboard_interrupt) {
        AVStream *in_stream, *out_stream;

        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0)
            break;

        in_stream = ifmt_ctx->streams[pkt.stream_index];
        if (pkt.stream_index >= stream_mapping_size ||
            stream_mapping[pkt.stream_index] < 0) {
            av_packet_unref(&pkt);
            continue;
        }

        pkt.stream_index = stream_mapping[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];

        /* rescale DTS to be monotonic increasing */
        int64_t dts;
        do {
            if (in_last_dts[pkt.stream_index] == AV_NOPTS_VALUE) {
                dts = 0;
                break;
            }

            if (pkt.dts > in_last_dts[pkt.stream_index]) {
                if (pkt.dts > in_last_dts[pkt.stream_index] + 1000) {
                    dts = out_last_dts[pkt.stream_index] + 10;
                } else {
                    dts = pkt.dts - in_last_dts[pkt.stream_index] + out_last_dts[pkt.stream_index];
                }
            } else {
                dts = out_last_dts[pkt.stream_index] + 10;
            }
        } while (0);
        in_last_dts[pkt.stream_index] = pkt.dts;
        out_last_dts[pkt.stream_index] = dts;

        /* shift pts */
        int64_t cts = pkt.pts - pkt.dts;
        pkt.dts = dts;
        pkt.pts = dts + cts;

        /* copy packet */
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;

        av_interleaved_write_frame(ofmt_ctx, &pkt);
        av_packet_unref(&pkt);
    }

    if (keyboard_interrupt) {
        av_log(ofmt_ctx, AV_LOG_WARNING, "%s\n", "Keyboard interrupt received");
        ret = AVERROR_EXIT;
    }

    av_write_trailer(ofmt_ctx);
end:

    avformat_close_input(&ifmt_ctx);

    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);

    av_dict_free(&options);
    av_freep(&stream_mapping);
    av_freep(&in_last_dts);
    av_freep(&out_last_dts);

    if (ret < 0 && ret != AVERROR_EOF) {
        return ret;
    }

    return 0;
}
