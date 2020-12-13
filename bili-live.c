#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <cJSON.h>

#include "bili-live.h"
// #include "remux.h"

static size_t max_api_size;
static char   *ffmpeg_headers;

static int keyboard_interrupt = 0;

static void handler_stop(int sig) {
    keyboard_interrupt = 1;
}

static int bili_log(const char *tag, const char *message, ...) {
    va_list args;
    va_start(args, message);

    struct tm *now = time_now();
    char fmt[4096];

    snprintf(fmt, 4095, "%4d-%02d-%02d %02d:%02d:%02d  [%s] %s",
                        now->tm_year + 1900, now->tm_mon, now->tm_mday,
                        now->tm_hour, now->tm_min, now->tm_sec,
                        tag, message);

    int rc = vfprintf(stderr, fmt, args);
    va_end(args);
    return rc;
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

static size_t write_to_file(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
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

    struct curl_slist *chunk = NULL;
    for (int i = 0; i < BILI_HTTP_HEADER_CNT; ++i) {
        chunk = curl_slist_append(chunk, BILI_HTTP_HEADERS[i]);
    }
    
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, chunk);

    return handle;
}

BILI_LIVE_ROOM *bili_make_room(uint32_t room_id) {
    BILI_LIVE_ROOM *room = (BILI_LIVE_ROOM *)malloc(sizeof(*room));

    room->room_id = room_id;
    room->handle = bili_make_handle();
    room->playurl_info = NULL;

    return room;
}

cJSON *bili_fetch_api(const BILI_LIVE_ROOM* room, int qn) {
    CURL     *handle = room->handle;
    CURLcode res;
    char     *api_url = bili_get_api_url(room, qn);
    memory   mem = (memory){NULL, 0};

    curl_easy_setopt(handle, CURLOPT_URL, api_url);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&mem);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_to_mem);

    res = curl_easy_perform(handle);

    if (CURL_FORMADD_OK != res) {
        fprintf(stderr, "cURL error\n");
        return NULL;
    }

    char *ct = NULL;
    curl_easy_getinfo(handle, CURLINFO_CONTENT_TYPE, &ct);

    cJSON *json = cJSON_Parse(mem.response);

    const cJSON *data = cJSON_GetObjectItem(json, "data");
    const cJSON *info = cJSON_GetObjectItem(data, "playurl_info");

    cJSON *api_data = cJSON_Duplicate(info, cJSON_True);

    free(api_url);
    free(mem.response);
    cJSON_Delete(json);

    return api_data;
}

char *bili_get_api_url(const BILI_LIVE_ROOM* room, int qn) {
    char *url = (char *)malloc(sizeof(char) * max_api_size);
    sprintf(url, BILI_XLIVE_API_V2, room->room_id, qn);
    return url;
}

void bili_free_room(BILI_LIVE_ROOM *room) {
    curl_easy_cleanup(room->handle);
    cJSON_Delete(room->playurl_info);
    free(room);
    bili_log("INFO", "Exit safely. Bye~\n");
}

int main(int argc, const char *argv[]) {
    max_api_size = strlen(BILI_XLIVE_API_V2) + 10 + 10 + 1;
    ffmpeg_headers = (char *)malloc(strlen(BILI_USER_AGENT) + strlen("User-Agent: ") + 3);
    sprintf(ffmpeg_headers, "User-Agent: %s\r\n", BILI_USER_AGENT);

    curl_global_init(CURL_GLOBAL_ALL);

    if (argc < 2) {
        printf("Usage: %s <room ID>\n", argv[0]);
        return 1;
    }

    uint32_t room_id;
    sscanf(argv[1], "%u", &room_id);
    BILI_LIVE_ROOM *room = bili_make_room(room_id);

    // (void)signal(SIGINT, handler_stop);
    int ret;
    while (!keyboard_interrupt) {
        if (bili_update_room(room)) {
            bili_log("INFO", "%u - Online\n", room->room_id);
            ret = bili_download_stream(room, DEFAULT);
            if (ret == -1) {
                break;
            }
        } else {
            bili_log("INFO", "%u - Offline. Waiting...\n", room->room_id);
            sleep(300);
        }
    }

    bili_free_room(room);
    curl_global_cleanup();
    free(ffmpeg_headers);
    return 0;
}

int bili_update_room(BILI_LIVE_ROOM* room) {
    cJSON_Delete(room->playurl_info);
    room->playurl_info = bili_fetch_api(room, 0);
    
    return !cJSON_IsNull(room->playurl_info);
}

int bili_download_stream(BILI_LIVE_ROOM* room, BILI_QUALITY_OPTION qn_option) {
    BILI_STREAM_CODEC codec;
    int qn;
    int ret;

    bili_find_codec_qn(&codec, &qn, room->playurl_info, qn_option);

    char *url = bili_get_stream_url(room, codec, qn);
    char filename[4096];
    struct tm *now = time_now();
    snprintf(filename, 4095, "%d-%02d-%02d_%02d%02d%02d-%u.flv",
                             now->tm_year + 1900, now->tm_mon, now->tm_mday,
                             now->tm_hour, now->tm_min, now->tm_sec,
                             room->room_id);
    // ret = remux(url, filename, ffmpeg_headers);
    FILE *fp = fopen(filename, "wb");
    if (fp) {
        CURL *handle = curl_easy_init();
        
        curl_easy_setopt(handle, CURLOPT_URL, url);

        curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(handle, CURLOPT_USERAGENT, BILI_USER_AGENT);

        curl_easy_setopt(handle, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_to_file);

        curl_easy_perform(handle);
        curl_easy_cleanup(handle);
    }
    free(url);

    return ret;
}

char *bili_get_stream_url(const BILI_LIVE_ROOM* room,
                          const BILI_STREAM_CODEC codec, const int qn) {
    cJSON *playurl_info = bili_fetch_api(room, qn);
    const cJSON *codecs = bili_get_codecs(playurl_info);

    const char *target_codec = BILI_CODEC_STR(codec);
    bili_log("INFO", "Downloading: stream %s, quality %d\n", target_codec, qn);

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
            fprintf(stderr, "Stream not found for %s. Fallback to %s\n",
                            "HEVC", "AVC");
            *codec = AVC;
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
