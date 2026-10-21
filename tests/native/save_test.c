#include "m_common_data.h"
#include "pc_save_bswap.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Diagnostics adapter for the isolated serializer test. */
void OSReport(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}
int main(int argc, char** argv) {
    assert(argc == 3);
    if (!strcmp(argv[1], "--create-fixture")) {
        Save_t* fixture = calloc(1, sizeof(*fixture));
        assert(fixture);
        fixture->scene_no = 0x1234;
        fixture->copy_protect = 0xabcd;
        fixture->land_info.id = 0x1357;
        memcpy(fixture->land_info.name, "V6-Test!", 8);
        fixture->melody = UINT64_C(0x0123456789abcdef);
        fixture->travel_hard_time = UINT64_C(0x1234567890abcdef);
        fixture->fruit = 0x2001;
        pc_save_bswap(fixture, PC_BSWAP_TO_BE);
        FILE* file = fopen(argv[2], "wb");
        assert(file && fwrite(fixture, 1, sizeof(*fixture), file) == sizeof(*fixture));
        assert(fclose(file) == 0);
        free(fixture);
        return 0;
    }
    FILE* input = fopen(argv[1], "rb");
    assert(input);
    Save_t* save = malloc(sizeof(*save));
    unsigned char* original = malloc(sizeof(*save));
    assert(save && original);
    assert(fread(save, 1, sizeof(*save), input) == sizeof(*save));
    assert(fclose(input) == 0);
    memcpy(original, save, sizeof(*save));
    pc_save_bswap(save, PC_BSWAP_FROM_BE);
    pc_save_bswap(save, PC_BSWAP_TO_BE);
    FILE* output = fopen(argv[2], "wb");
    assert(output);
    assert(fwrite(save, 1, sizeof(*save), output) == sizeof(*save));
    assert(fclose(output) == 0);
    if (memcmp(save, original, sizeof(*save))) {
        size_t differences = 0;
        for (size_t i = 0; i < sizeof(*save); ++i)
            if (((unsigned char*)save)[i] != original[i]) ++differences;
        fprintf(stderr, "Round-trip differences: %zu\n", differences);
        return 1;
    }
    free(save);
    free(original);
    return 0;
}
