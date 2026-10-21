#include "jaudio_NES/dvdthread.h"
#include "dolphin/ar.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

/* Only the disc backend is a fixture. The DVD queue, audio loader, ARQ/DMA
 * and task copies are the same sources used by the Android engine. */
enum { AUDIO_SIZE = 0x8000 + 17, ARAM_OFFSET = 0x1000 };
static u8 audio[AUDIO_SIZE];
static unsigned reads, opens, closes, callbacks;
static u32 callback_owner;

s32 DVDConvertPathToEntrynum(char* path) {
    return strcmp(path, "/audiorom.img") == 0 ? 7 : -1;
}

BOOL DVDFastOpen(s32 entry, DVDFileInfo* info) {
    assert(entry == 7);
    memset(info, 0, sizeof(*info));
    info->length = sizeof(audio);
    ++opens;
    return TRUE;
}

BOOL DVDOpen(char* path, DVDFileInfo* info) {
    s32 entry = DVDConvertPathToEntrynum(path);
    return entry >= 0 ? DVDFastOpen(entry, info) : FALSE;
}

s32 DVDReadPrio(DVDFileInfo* info, void* destination, s32 length, s32 offset, s32 priority) {
    assert(info->length == sizeof(audio) && length > 0 && offset >= 0);
    assert((u32)offset < sizeof(audio) && priority == 2);
    size_t available = sizeof(audio) - (u32)offset;
    size_t copied = (size_t)length < available ? (size_t)length : available;
    memset(destination, 0, (size_t)length);
    memcpy(destination, audio + offset, copied);
    ++reads;
    return (s32)copied;
}

BOOL DVDClose(DVDFileInfo* info) {
    assert(info->length == sizeof(audio));
    ++closes;
    return TRUE;
}

static void finished(u32 owner) {
    callback_owner = owner;
    ++callbacks;
}

int main(void) {
    u8 destination[AUDIO_SIZE + 31];
    u32 status = UINT32_MAX;
    for (unsigned i = 0; i < sizeof(audio); ++i) audio[i] = (u8)(i * 37 + 11);
    ARInit(NULL, 0);
    jac_dvdproc_init();

    /* This is the production boot path used by pc_neos_init_sync(). It queues
     * two MRAM -> ARAM transfers with a null ARQRequest, including a padded tail. */
    DVDT_LoadtoARAM(42, "/audiorom.img", ARAM_OFFSET, 0, 0, &status, finished);
    assert(status == 0 && callbacks == 0 && reads == 0);
    pc_dvd_process_all_tasks();
    assert(status == AUDIO_SIZE && callbacks == 1 && callback_owner == 42);
    assert(reads == 2 && opens == 1 && closes == 1);

    memset(destination, 0xff, sizeof(destination));
    DVDT_ARAMtoDRAM(43, (uintptr_t)destination, ARAM_OFFSET, AUDIO_SIZE + 15, &status, finished);
    pc_dvd_process_all_tasks();
    assert(status == AUDIO_SIZE + 15 && callbacks == 2 && callback_owner == 43);
    assert(memcmp(audio, destination, AUDIO_SIZE) == 0);
    for (unsigned i = AUDIO_SIZE; i < AUDIO_SIZE + 15; ++i) assert(destination[i] == 0);

    /* Exercise the reverse task with a native process pointer and an ARAM
     * offset. Neither is serialized into the other address space. */
    for (unsigned i = 0; i < sizeof(audio); ++i) audio[i] ^= 0x5a;
    DVDT_DRAMtoARAM(44, (uintptr_t)audio, ARAM_OFFSET, AUDIO_SIZE, &status, finished);
    pc_dvd_process_all_tasks();
    DVDT_ARAMtoDRAM(45, (uintptr_t)destination, ARAM_OFFSET, AUDIO_SIZE, &status, finished);
    pc_dvd_process_all_tasks();
    assert(callbacks == 4 && callback_owner == 45 && status == AUDIO_SIZE);
    assert(memcmp(audio, destination, AUDIO_SIZE) == 0);

    /* Queued completion must copy the native pointer fields as well. */
    status = UINT32_MAX;
    assert(DVDT_CheckPass(46, &status, finished));
    pc_dvd_process_all_tasks();
    assert(status == 46 && callbacks == 5 && callback_owner == 46);

    DVDT_LoadtoARAM(47, "/missing.img", ARAM_OFFSET, 0, 0, &status, finished);
    pc_dvd_process_all_tasks();
    assert(status == UINT32_MAX && callbacks == 6 && callback_owner == UINT32_MAX);
    assert(opens == closes && opens == 1);
    return 0;
}
