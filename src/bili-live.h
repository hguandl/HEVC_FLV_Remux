#ifndef BILI_LIVE_H
#define BILI_LIVE_H

#include <stdint.h>
#include <time.h>

#include <cJSON.h>
#include <curl/curl.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BILI_USER_AGENT (\
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_6)"\
        " AppleWebKit/605.1.15 (KHTML, like Gecko)"\
        " Version/14.0.1 Safari/605.1.15")

#define BILI_XLIVE_API_V2 (\
        "https://api.live.bilibili.com/xlive/web-room/v2/index"\
        "/getRoomPlayInfo?"\
        "room_id=%u"\
        "&protocol=0,1"\
        "&format=0,2"\
        "&codec=0,1"\
        "&qn=%d"\
        "&platform=web"\
        "&ptype=16")

const char *BILI_HTTP_HEADERS[] = {
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9",
    "Accept-Language: zh-cn",
};

const size_t BILI_HTTP_HEADER_CNT = 2;

typedef struct {
    char   *response;
    size_t size;
} memory;

typedef struct {
    uint32_t room_id;
    CURL     *handle;
    cJSON    *playurl_info;
} BILI_LIVE_ROOM;

CURL *bili_make_handle();

BILI_LIVE_ROOM *bili_make_room(uint32_t room_id);

void bili_free_room(BILI_LIVE_ROOM *room);

cJSON *bili_fetch_api(const BILI_LIVE_ROOM* room, int qn);

char *bili_get_api_url(const BILI_LIVE_ROOM* room, int qn);

int bili_update_room(BILI_LIVE_ROOM* room);

typedef enum {
    // Use best quality. If both codecs have the same quality:
    HEVC_PRIORITY,
    AVC_PRIORITY,

    // Use the best quality from specified codec.
    // If not exists, fallback to the other one.
    HEVC_ONLY,
    AVC_ONLY

    #define DEFAULT HEVC_PRIORITY

    // TODO: Support custom options
} BILI_QUALITY_OPTION;

typedef enum {
    AVC,
    HEVC
} BILI_STREAM_CODEC;

const char *BILI_CODEC_STR(BILI_STREAM_CODEC codec) {
    switch (codec) {
        case AVC:
        return "avc";
        case HEVC:
        return "hevc";
        default:
        return NULL;
    }
}

int bili_download_stream(BILI_LIVE_ROOM* room, BILI_QUALITY_OPTION qn_option);

void bili_find_codec_qn(BILI_STREAM_CODEC *codec,
                        int *qn,
                        cJSON *playurl_info, BILI_QUALITY_OPTION qn_option);

char *bili_get_stream_url(const BILI_LIVE_ROOM* room,
                          const BILI_STREAM_CODEC codec, const int qn);

static const cJSON *bili_get_codecs(cJSON *playurl_info);

struct tm *time_now() {
    time_t now =time(NULL);
    return localtime(&now);
}

#ifdef __cplusplus
}
#endif

#endif
