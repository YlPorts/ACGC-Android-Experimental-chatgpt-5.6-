#include "dolphin/ar.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static int callbacks;
static void finished(uintptr_t arg) {
    ARQRequest* req = (ARQRequest*)arg;
    assert(req->owner == 123 && req->type == ARAM_DIR_ARAM_TO_MRAM);
    assert(req->length == 96 && req->source == 32 && req->dest != 0);
    ++callbacks;
}
int main(void) {
    u8* source = malloc(96);
    u8* destination = malloc(96);
    assert(source && destination);
    for (int i = 0; i < 96; ++i) source[i] = (u8)(i * 17);
    ARInit(NULL, 0);
    assert(ARGetSize() == 16 * 1024 * 1024 && ARCheckInit());
    ARStartDMA(ARAM_DIR_MRAM_TO_ARAM, (uintptr_t)source, 32, 96);
    ARQRequest req;
    ARQPostRequest(&req, 123, ARAM_DIR_ARAM_TO_MRAM, 0, 32, (uintptr_t)destination, 96, finished);
    assert(callbacks == 1 && memcmp(source, destination, 96) == 0);
    memset(destination, 42, 96);
    ARStartDMA(ARAM_DIR_ARAM_TO_MRAM, (uintptr_t)destination, ARGetSize() - 32, 96);
    for (int i = 0; i < 96; ++i) assert(destination[i] == 0);
    assert(ARAlloc((u32)-1) == 0);
    free(source);
    free(destination);
    return 0;
}
