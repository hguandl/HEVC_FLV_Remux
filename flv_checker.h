#ifndef FLV_CHECKER_H
#define FLV_CHECKER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#define FLV_TAGTYPE_AUDIO  8
#define FLV_TAGTYPE_VIDEO  9
#define FLV_TAGTYPE_SCRIPT 18

static const uint8_t duration_header[] = {
    0x08, 0x64, 0x75, 0x72, 0x61, 0x74, 0x69, 0x6f, 0x6e
};

uint32_t check(FILE *origin, FILE *dest);

uint32_t fix_ts(FILE *dest, uint32_t dts, uint8_t tag_index);

void change_duration(FILE *dest, double duration);

#ifdef __cplusplus
}
#endif

#endif