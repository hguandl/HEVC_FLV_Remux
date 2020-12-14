#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "flv_checker.h"

int main(int argc, const char *argv[]) {
    FILE *in_file, *out_file;

    in_file = fopen(argv[1], "rb");
    out_file = fopen(argv[2], "wb+");

    if (in_file && out_file) {
        uint32_t last_dts = check(in_file, out_file);
        fclose(in_file);
        printf("Duration: %lf\n", (double)last_dts / 1000.0);
        change_duration(out_file, (double)last_dts / 1000.0);
        fclose(out_file);
    }

    return 0;
}

static uint32_t in_last_dts[2];
static uint32_t out_last_dts[2];

static uint8_t in_no_dts[2];

uint32_t check(FILE *origin, FILE *dest) {
    uint8_t buf[4096];

    in_no_dts[0] = in_no_dts[1] = 1;

    /* copy FLV header */
    fread(buf, 1, 9, origin);
    fwrite(buf, 1, 9, dest);

    while (1) {
        fread(buf, 1, 5, origin);
        uint8_t tag_type = buf[4];
        /* invalid tag type */
        if (tag_type != FLV_TAGTYPE_AUDIO
            && tag_type != FLV_TAGTYPE_VIDEO
            && tag_type != FLV_TAGTYPE_SCRIPT) {
            break;
        }

        fwrite(buf, 1, 5, dest);

        fread(buf, 1, 3, origin);
        uint32_t data_size = (buf[0] << 16) + (buf[1] << 8) + buf[2];
        fwrite(buf, 1, 3, dest);

        fread(buf, 1, 4, origin);
        uint32_t dts = (buf[0] << 16) + (buf[1] << 8) + buf[2] + (buf[3] << 24);

        uint32_t out_dts = 0;

        if (tag_type == FLV_TAGTYPE_AUDIO || tag_type == FLV_TAGTYPE_VIDEO) {
            out_dts = fix_ts(dest, dts, tag_type - 8);
        }

        // printf("%u -> %u\n", dts, out_dts);

        uint8_t dts_bytes[4];
        dts_bytes[0] = (out_dts >> 16) & 0xff;
        dts_bytes[1] = (out_dts >> 8) & 0xff;
        dts_bytes[2] = out_dts & 0xff;
        dts_bytes[3] = (out_dts >> 24) & 0xff;

        fwrite(dts_bytes, 1, 4, dest);

        size_t size_left = data_size + 3;
        while (size_left > 0) {
            size_t size_read = size_left > 4096 ? 4096 : size_left;
            fread(buf, 1, size_read, origin);
            fwrite(buf, 1, size_read, dest);
            size_left -= size_read;
        }

    }

    return out_last_dts[0];
}

uint32_t fix_ts(FILE *dest, uint32_t dts, uint8_t tag_index) {
    do {
        if (in_no_dts[tag_index]) {
            out_last_dts[tag_index] = 0;
            in_no_dts[tag_index] = 0;
            break;
        }

        if (dts >= in_last_dts[tag_index]) {
            if (dts > in_last_dts[tag_index] + 1000) {
                out_last_dts[tag_index] += 10;
            } else {
                out_last_dts[tag_index] = dts - in_last_dts[tag_index] + out_last_dts[tag_index];
            }
        } else {
            if (in_last_dts[tag_index] - dts < 5000) {
                uint32_t dts_new = dts - in_last_dts[tag_index] + out_last_dts[tag_index];
                if (dts_new < 0) {
                    dts_new = 1;
                }
                out_last_dts[tag_index] = dts_new;
            } else {
                out_last_dts[tag_index] += 10;
            }
        }
    } while (0);
    in_last_dts[tag_index] = dts;
    return out_last_dts[tag_index];
}

void change_duration(FILE *dest, double duration) {
    fseek(dest, 0, SEEK_SET);

    union {
        double value;
        uint8_t bytes[8];
    } double_struct;
    double_struct.value = duration;

    uint8_t duration_bytes[9];
    duration_bytes[0] = 0x00;
    for (int i = 1; i < 9; ++i) {
        duration_bytes[i] = double_struct.bytes[8 - i];
    }

    uint8_t buf[1024 * 20];
    fread(buf, 1024, 20, dest);
    for (int i = 0; i < 1024 * 20; ++i) {
        if (buf[i] == duration_header[0]) {
            uint8_t found = 1;
            for (int j = 1; j < 9; ++j) {
                if (buf[i + j] != duration_header[j]) {
                    found = 0;
                    break;
                }
            }
            if (found) {
                fseek(dest, i + 9 + 1, SEEK_SET);
                fwrite(duration_bytes, 1, 9, dest);
                break;
            }
        }
    }
}
