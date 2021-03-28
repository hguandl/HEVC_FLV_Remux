#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <libavutil/common.h>
#include <libavutil/error.h>

#include "bili-live.h"
#include "remux.h"

static int bili_log(const char *tag, const bool update, const char *message, ...) {
    va_list args;
    va_start(args, message);

    struct tm *now = time_now();
    char fmt[4096], *log_template;

    if (update) {
        log_template = "%4d-%02d-%02d %02d:%02d:%02d  [%s] %s\r";
    } else {
        log_template = "%4d-%02d-%02d %02d:%02d:%02d  [%s] %s\n";
    }

    snprintf(fmt, 4095, log_template,
                        now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,
                        now->tm_hour, now->tm_min, now->tm_sec,
                        tag, message);

    int rc = vfprintf(stderr, fmt, args);

    if (update) {
        fflush(stderr);
    }

    va_end(args);
    return rc;
}

static void print_usage(const char *argv0) {
    static const char *format =
        "Usage: %s [-qh] [-o <quality option>] [-d <log path>] <room ID>\n"
        "\n-q:  fetch API only\n"
        "-h:  print usage\n"
        "\nQuality options:\n"
        "%d    HEVC_PRIORITY (default)\n"
        "%d    AVC_PRIORITY\n"
        "%d    HEVC_ONLY\n"
        "%d    AVC_ONLY\n";

    fprintf(stderr, format,
            argv0,
            HEVC_PRIORITY,
            AVC_PRIORITY,
            HEVC_ONLY,
            AVC_ONLY);
}

static size_t write_to_mem(void *data, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    memory *mem = (memory *)userp;

    char *ptr = realloc(mem->response, mem->size + realsize + 1);

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), data, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;

    return realsize;
}

CURL *bili_make_handle() {
    CURL *handle = curl_easy_init();

    /* Important: use HTTP2 over HTTPS */
    curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);

    /* For completeness */
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 2L);

    curl_easy_setopt(handle, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(handle, CURLOPT_USERAGENT, BILI_USER_AGENT);

    return handle;
}

BILI_LIVE_ROOM *bili_make_room(uint32_t room_id) {
    BILI_LIVE_ROOM *room = (BILI_LIVE_ROOM *)malloc(sizeof(*room));

    room->room_id = room_id;
    room->handle = bili_make_handle();
    room->referer = (char *)malloc(sizeof(char) * 4096);
    room->playurl_info = NULL;

    struct curl_slist *curl_headers = NULL;
    for (int i = 0; i < BILI_HTTP_HEADER_CNT; ++i) {
        curl_headers = curl_slist_append(curl_headers, BILI_HTTP_API_HEADERS[i]);
    }
    sprintf(room->referer, "Referer: https://live.bilibili.com/%u", room_id);
    curl_slist_append(curl_headers, room->referer);
    curl_easy_setopt(room->handle, CURLOPT_HTTPHEADER, curl_headers);
    room->curl_headers = curl_headers;

    room->ffmpeg_headers = (char *)malloc(sizeof(char) * 4096);
    sprintf(room->ffmpeg_headers, "%s\r\nReferer: %s\r\n",
                                  BILI_HTTP_FLV_HEADERS, room->referer);

    return room;
}

cJSON *bili_fetch_api(const BILI_LIVE_ROOM *room, int qn) {
    CURL     *handle = room->handle;
    CURLcode res;
    char     *api_url = bili_get_api_url(room, qn);
    memory   mem = (memory){NULL, 0};

    curl_easy_setopt(handle, CURLOPT_URL, api_url);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&mem);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_to_mem);

    res = curl_easy_perform(handle);
    int retry = 5;

    while (CURL_FORMADD_OK != res && retry) {
        bili_log("ERROR", false, "cURL error: %s", curl_easy_strerror(res));
        res = curl_easy_perform(handle);
        --retry;
    }

    cJSON *json = cJSON_Parse(mem.response);

    const cJSON *data = cJSON_GetObjectItem(json, "data");
    const cJSON *info = cJSON_GetObjectItem(data, "playurl_info");

    cJSON *api_data = cJSON_Duplicate(info, cJSON_True);

    free(api_url);
    free(mem.response);
    cJSON_Delete(json);

    return api_data;
}

char *bili_get_api_url(const BILI_LIVE_ROOM *room, int qn) {
    char *url = (char *)malloc(sizeof(char) * 4096);
    sprintf(url, BILI_XLIVE_API_V2, room->room_id, qn);
    return url;
}

void bili_free_room(BILI_LIVE_ROOM *room) {
    curl_slist_free_all(room->curl_headers);
    curl_easy_cleanup(room->handle);
    free(room->referer);
    free(room->ffmpeg_headers);
    cJSON_Delete(room->playurl_info);
    free(room);
    bili_log("INFO", false, "Exit safely. Bye~");
}

int main(int argc, const char *argv[]) {
    curl_global_init(CURL_GLOBAL_ALL);

    int ch, bili_qo = 0;
    bool qoption = false;
    char log_path[BUFSIZ] = { 0 };
    while ((ch = getopt(argc, (char **)argv, "hqo:d:")) != -1) {
        switch (ch) {
            case 'o':
                bili_qo = atoi(optarg);
                break;
            case 'q':
                qoption = true;
                break;
            case 'd':
                if (strlen(optarg) > BUFSIZ) {
                    bili_log("ERROR", false, "Log path too long");
                    return 1;
                }
                strncpy(log_path, optarg, BUFSIZ);
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                return 0;
        }
    }

    if (bili_qo < 0 || bili_qo > 3) {
        bili_log("WARN", false, "Quality option not valid");
        print_usage(argv[0]);
        return 1;
    }

    if (argc - optind <= 0) {
        bili_log("WARN", false, "Room ID not provided");
        print_usage(argv[0]);
        return 1;
    }

    if (strlen(log_path) > 0) {
        char log_env[BUFSIZ + 64];
        sprintf(log_env, "FFREPORT=file=%s/%%p-%%t.log:level=32", log_path);
        putenv(log_env);
    }

    uint32_t room_id;
    room_id = strtol(argv[optind], NULL, 10);
    BILI_LIVE_ROOM *room = bili_make_room(room_id);

    if (qoption) {
        printf("%s\n", cJSON_Print(bili_fetch_api(room, 0)));
        bili_free_room(room);
        curl_global_cleanup();
        return 0;
    }

    (void)signal(SIGCHLD, SIG_IGN);
    int ret, retry = 5;
    while (1) {
        if (bili_update_room(room)) {
            ret = bili_download_stream(room, bili_qo);
            if (ret == AVERROR_EXIT) {
                break;
            }
            if (ret < 0) {
                --retry;
                if (retry <= 0) {
                    retry = 10;
                    sleep(10);
                }
            }
        } else {
            bili_log("INFO", true, "%u - Offline. Waiting...", room->room_id);
            sleep(30);
        }
    }

    bili_free_room(room);
    curl_global_cleanup();
    return 0;
}

bool bili_update_room(BILI_LIVE_ROOM *room) {
    cJSON_Delete(room->playurl_info);
    room->playurl_info = bili_fetch_api(room, 0);

    return room->playurl_info && !cJSON_IsNull(room->playurl_info);
}

int bili_download_stream(BILI_LIVE_ROOM *room, BILI_QUALITY_OPTION qn_option) {
    BILI_STREAM_CODEC codec;
    int qn;
    int ret;
    bool transcode_to_hevc = false;

    bili_find_codec_qn(&codec, &qn, room->playurl_info, qn_option);

    if (codec == AVC2HEVC) {
        codec = AVC;
        transcode_to_hevc = true;
    }

    char *url = bili_get_stream_url(room, codec, qn);
    char filename[4096];
    struct tm *now = time_now();
    snprintf(filename, 4095, "%d%02d%02d_%02d%02d%02d-%u.mp4",
                             now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,
                             now->tm_hour, now->tm_min, now->tm_sec,
                             room->room_id);
    ret = remux(url, filename, room->ffmpeg_headers);

    free(url);

    if (ret < 0) {
        bili_log("ERROR", false, "%s", av_err2str(ret));
    }

    if (transcode_to_hevc) {
        pid_t child = fork();

        if (child == -1) {
            bili_log("ERROR", false, "Cannot fork process.");
        }

        if (child == 0) {
            int ffmpeg_ret;
            char new_filename[4096];
            strncpy(new_filename, filename, 4095);

            char *ext = strstr(new_filename, ".mp4");
            const char *suffix = "-hevc.mp4";

            for (int i = 0; i < 9; ++i) {
                *ext = suffix[i];
                ++ext;
            }
            *ext = '\0';

            bili_log("INFO", false, "Transcoding to %s", new_filename);

            pid_t grandchild = fork();

            if (grandchild == -1) {
                bili_log("ERROR", false, "Cannot fork process.");
                exit(-1);
            }

            if (grandchild == 0) {
                ffmpeg_ret = execlp("ffmpeg", "ffmpeg",
                                    "-nostdin",
                                    "-loglevel", "quiet",
                                    "-i", filename,
                                    "-c:v", "libx265",
                                    "-x265-params", "log-level=none",
                                    "-pix_fmt", "yuv420p10le",
                                    "-tag:v", "hvc1",
                                    "-max_muxing_queue_size", "4096",
                                    "-c:a", "copy",
                                    new_filename,
                                    NULL);
                if (ffmpeg_ret) {
                    bili_log("ERROR", false, "FFmpeg not found.");
                }
            } else {
                wait(&ffmpeg_ret);
                if (ffmpeg_ret == 0) {
                    bili_log("INFO", false, "Transcoding done. Removing %s", filename);
                    int del_ret = remove(filename);
                    if (del_ret) {
                        bili_log("ERROR", false, "Cannot delete %s", filename);
                    }
                } else {
                    bili_log("ERROR", false, "Transcode failed.");
                }
                return AVERROR_EXIT;
            }
        }
    }

    return ret;
}

char *bili_get_stream_url(const BILI_LIVE_ROOM *room,
                          const BILI_STREAM_CODEC codec, const int qn) {
    cJSON *playurl_info = bili_fetch_api(room, qn);
    const cJSON *codecs = bili_get_codecs(playurl_info);

    const char *target_codec = BILI_CODEC_STR(codec);
    bili_log("INFO", false, "Downloading: stream %s, quality %d", target_codec, qn);

    char *url = NULL;

    for (int i = 0; i < cJSON_GetArraySize(codecs); ++i) {
        const cJSON *codec = cJSON_GetArrayItem(codecs, i);
        const cJSON *codec_name = cJSON_GetObjectItem(codec, "codec_name");

        if (!strcasecmp(codec_name->valuestring, target_codec)) {
            const cJSON *url_infos = cJSON_GetObjectItem(codec, "url_info");
            const cJSON *url_info = cJSON_GetArrayItem(url_infos, 0);

            const char *base_url = cJSON_GetObjectItem(codec, "base_url")->valuestring;
            const char *host = cJSON_GetObjectItem(url_info, "host")->valuestring;
            const char *extra = cJSON_GetObjectItem(url_info, "extra")->valuestring;

            int size = strlen(host) + strlen(base_url) + strlen(extra) + 1;
            url = (char *)malloc(sizeof(char) * size);
            sprintf(url, "%s%s%s", host, base_url, extra);

            break;
        }
    }

    cJSON_Delete(playurl_info);
    return url;
}

static const cJSON *bili_get_codecs(cJSON *playurl_info) {
    const cJSON *playurl = cJSON_GetObjectItem(playurl_info, "playurl");
    const cJSON *streams = cJSON_GetObjectItem(playurl, "stream");
    const cJSON *stream = cJSON_GetArrayItem(streams, 0);
    const cJSON *formats = cJSON_GetObjectItem(stream, "format");
    const cJSON *format = cJSON_GetArrayItem(formats, 0);
    const cJSON *codecs = cJSON_GetObjectItem(format, "codec");
    return codecs;
}

void bili_find_codec_qn(BILI_STREAM_CODEC *codec,
                        int  *qn,
                        cJSON *playurl_info, BILI_QUALITY_OPTION qn_option) {
    int avc_best_qn = 0;
    int hevc_best_qn = 0;

    const cJSON *codecs = bili_get_codecs(playurl_info);

    for (int i = 0; i < cJSON_GetArraySize(codecs); ++i) {
        const cJSON *codec = cJSON_GetArrayItem(codecs, i);
        const cJSON *codec_name = cJSON_GetObjectItem(codec, "codec_name");
        const cJSON *accept_qn = cJSON_GetObjectItem(codec, "accept_qn");
        const cJSON *best_qn = cJSON_GetArrayItem(accept_qn, 0);

        if (!strcasecmp(codec_name->valuestring, "avc")) {
            avc_best_qn = best_qn->valueint;
            continue;
        }

        if (!strcasecmp(codec_name->valuestring, "hevc")) {
            hevc_best_qn = best_qn->valueint;
            continue;
        }
    }

    if (qn_option == HEVC_PRIORITY || qn_option == AVC_PRIORITY) {
        if (avc_best_qn > hevc_best_qn) {
            *codec = AVC;
            *qn = avc_best_qn;
            return;
        }

        if (avc_best_qn < hevc_best_qn) {
            *codec = HEVC;
            *qn = hevc_best_qn;
            return;
        }

        if (avc_best_qn == hevc_best_qn) {
            *qn = avc_best_qn;
            if (qn_option == HEVC_PRIORITY) {
                *codec = HEVC;
            } else {
                *codec = AVC;
            }
            return;
        }
    }

    if (qn_option == HEVC_ONLY) {
        if (hevc_best_qn > 0) {
            *codec = HEVC;
            *qn = hevc_best_qn;
            return;
        } else {
            bili_log("WARN", false, "Stream not found for %s. Transcode from %s",
                            "HEVC", "AVC");
            *codec = AVC2HEVC;
            *qn = avc_best_qn;
            return;
        }
    }

    if (qn_option == AVC_ONLY) {
        *codec = AVC;
        *qn = avc_best_qn;
        return;
    }
}
