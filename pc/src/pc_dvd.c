/* pc_dvd.c - DVD filesystem: reads from disc image (CISO/ISO/GCM) or extracted files */
#include "pc_platform.h"
#include "dolphin/dvd.h"
#include "pc_disc.h"
#include "pc_language.h"

/* Keep the legacy exported function as well as the public header's macro. */
#undef DVDGetFileInfoStatus

static DVDDiskID disk_id = {
    {'G', 'A', 'F', 'E'},
    {'0', '1'},
    0, 0,
    0, 0,
    {0}
};

DVDDiskID* DVDGetCurrentDiskID(void) { return &disk_id; }

#define MAX_DVD_ENTRIES 512

static struct {
    char path[256];
    int  used;
} dvd_entry_table[MAX_DVD_ENTRIES];
static int dvd_entry_count = 0;

/* File-based fallback path (only used when no disc image) */
static char assets_base_path[512] = {0};
static int assets_fallback_inited = 0;

static void dvd_init_fallback_path(void) {
    if (assets_fallback_inited) return;
    assets_fallback_inited = 1;

    const char* candidates[] = {
        "assets/files",
        "assets",
        "../assets/files",
        "../assets",
        "../../assets/files",
        "../../assets",
    };
    for (int i = 0; i < (int)(sizeof(candidates)/sizeof(candidates[0])); i++) {
        char test[768];
        snprintf(test, sizeof(test), "%s/COPYDATE", candidates[i]);
        FILE* f = fopen(test, "rb");
        if (f) {
            fclose(f);
            strncpy(assets_base_path, candidates[i], sizeof(assets_base_path)-1);
            assets_base_path[sizeof(assets_base_path)-1] = '\0';
            return;
        }
    }
    strncpy(assets_base_path, "assets", sizeof(assets_base_path)-1);
    assets_base_path[sizeof(assets_base_path)-1] = '\0';
}

s32 DVDConvertPathToEntrynum(char* path) {
    for (int i = 0; i < dvd_entry_count; i++) {
        if (dvd_entry_table[i].used && strcmp(dvd_entry_table[i].path, path) == 0) {
            return i;
        }
    }

    if (dvd_entry_count >= MAX_DVD_ENTRIES) {
        fprintf(stderr, "[PC/DVD] Entry table full (%d entries)! Cannot register: %s\n",
                MAX_DVD_ENTRIES, path);
        return -1;
    }

    int idx = dvd_entry_count++;
    strncpy(dvd_entry_table[idx].path, path, sizeof(dvd_entry_table[idx].path) - 1);
    dvd_entry_table[idx].path[sizeof(dvd_entry_table[idx].path) - 1] = '\0';
    dvd_entry_table[idx].used = 1;
    return idx;
}

/* DVDFileInfo is a native runtime object, not a serialized GC structure.
 * Preserve the PC backend's FILE* in cb.addr without assuming its ABI offset. */
/* For disc-image backed files, FILE* is set to sentinel DISC_SENTINEL,
 * and the disc offset is stored in startAddr (0x30). */
#define DISC_SENTINEL ((FILE*)(uintptr_t)0xDEADC0DE)

static void dvd_info_init(DVDFileInfo* info, FILE* stream, u32 offset, u32 length) {
    memset(info, 0, sizeof(*info));
    info->cb.addr = stream;
    info->startAddr = offset;
    info->length = length;
}

BOOL DVDFastOpen(s32 entrynum, DVDFileInfo* fileInfo) {
    if (entrynum < 0 || entrynum >= dvd_entry_count || !dvd_entry_table[entrynum].used) {
        return FALSE;
    }

    const char* path = dvd_entry_table[entrynum].path;

    /* A regional audiorom may only replace the USA file when pc_language has
     * already validated the matching PAL ArcHeader set. This check happens
     * before the disc lookup so Jac_CheckFile/DVDT_LoadtoARAM see the same
     * external file and size. */
    if ((strcmp(path, "/audiorom.img") == 0 || strcmp(path, "audiorom.img") == 0) &&
        pc_language_audio_override_enabled()) {
        const char* audio_path = pc_language_audio_rom_path();
        FILE* fp = audio_path ? fopen(audio_path, "rb") : NULL;
        if (fp) {
            long length;
            if (fseek(fp, 0, SEEK_END) == 0 && (length = ftell(fp)) > 0 &&
                fseek(fp, 0, SEEK_SET) == 0) {
                dvd_info_init(fileInfo, fp, 0, (u32)length);
                return TRUE;
            }
            fclose(fp);
        }
        /* Validation succeeded earlier, but if the file disappeared or became
         * unreadable, fall back to the USA disc instead of failing audio boot. */
        fprintf(stderr, "[Language/Audio] Regional audiorom became unavailable; falling back to USA\n");
        pc_language_audio_fallback_to_usa();
    }

    /* Try disc image first */
    if (pc_disc_is_open()) {
        u32 disc_off, disc_sz;
        if (pc_disc_find_file(path, &disc_off, &disc_sz)) {
            dvd_info_init(fileInfo, DISC_SENTINEL, disc_off, disc_sz);
            return TRUE;
        }
    }

    /* Fall back to extracted files */
    dvd_init_fallback_path();
    {
        char fullpath[768];
        FILE* fp;
        u32 len;

        if (path[0] == '/') {
            snprintf(fullpath, sizeof(fullpath), "%s%s", assets_base_path, path);
        } else {
            snprintf(fullpath, sizeof(fullpath), "%s/%s", assets_base_path, path);
        }

        fp = fopen(fullpath, "rb");
        if (!fp) {
            return FALSE;
        }

        fseek(fp, 0, SEEK_END);
        len = (u32)ftell(fp);
        fseek(fp, 0, SEEK_SET);

        dvd_info_init(fileInfo, fp, 0, len);
    }

    return TRUE;
}

BOOL DVDOpen(char* filename, DVDFileInfo* fileInfo) {
    s32 entry = DVDConvertPathToEntrynum(filename);
    if (entry < 0) return FALSE;
    return DVDFastOpen(entry, fileInfo);
}

BOOL DVDClose(DVDFileInfo* fileInfo) {
    FILE* fp = fileInfo->cb.addr;
    if (fp && fp != DISC_SENTINEL) {
        fclose(fp);
    }
    fileInfo->cb.addr = NULL;
    return TRUE;
}

s32 DVDReadPrio(DVDFileInfo* fileInfo, void* buf, s32 length, s32 offset, s32 prio) {
    FILE* fp = fileInfo->cb.addr;
    (void)prio;

    if (fp == DISC_SENTINEL) {
        /* disc image read */
        u32 base = fileInfo->startAddr;
        if (pc_disc_read(base + (u32)offset, buf, (u32)length))
            return length;
        return -1;
    }

    if (!fp) {
        return -1;
    }

    fseek(fp, offset, SEEK_SET);
    return (s32)fread(buf, 1, length, fp);
}

s32 DVDRead(DVDFileInfo* fileInfo, void* buf, s32 length, s32 offset) {
    return DVDReadPrio(fileInfo, buf, length, offset, 2);
}

u32 DVDGetLength(DVDFileInfo* fileInfo) {
    return fileInfo->length;
}

BOOL DVDReadAsyncPrio(DVDFileInfo* fileInfo, void* buf, s32 length, s32 offset,
                      DVDCallback callback, s32 prio) {
    s32 nread = DVDReadPrio(fileInfo, buf, length, offset, prio);
    if (callback) {
        callback(nread, fileInfo);
    }
    return TRUE;
}

void OSDVDFatalError(void) {
    fprintf(stderr, "[PC/DVD] Fatal DVD error\n");
}

void DVDInit(void) {
    /* disc image init is done in pc_main.c via pc_disc_init() */
}

void DVDSetAutoFatalMessaging(BOOL enable) { (void)enable; }

s32 DVDGetFileInfoStatus(DVDFileInfo* fileInfo) {
    (void)fileInfo;
    return 0;
}

s32 DVDGetTransferredSize(DVDFileInfo* fileInfo) {
    (void)fileInfo;
    return 0;
}

BOOL DVDFastClose(DVDFileInfo* fileInfo) {
    return DVDClose(fileInfo);
}

s32 DVDGetDriveStatus(void) { return 0; }
s32 DVDCancel(volatile DVDCommandBlock* block) { (void)block; return 0; }
BOOL DVDCancelAsync(DVDCommandBlock* block, DVDCBCallback callback) { (void)block; (void)callback; return TRUE; }
s32 DVDChangeDisk(DVDCommandBlock* block, DVDDiskID* id) { (void)block; (void)id; return 0; }
BOOL DVDChangeDiskAsync(DVDCommandBlock* block, DVDDiskID* id, DVDCBCallback callback) { (void)block; (void)id; (void)callback; return TRUE; }
s32 DVDGetCommandBlockStatus(const DVDCommandBlock* block) { (void)block; return 0; }

BOOL DVDPrepareStreamAsync(DVDFileInfo* fi, u32 len, u32 off, DVDCallback cb) {
    (void)fi; (void)len; (void)off; (void)cb;
    return TRUE;
}
s32 DVDCancelStream(DVDCommandBlock* block) { (void)block; return 0; }
