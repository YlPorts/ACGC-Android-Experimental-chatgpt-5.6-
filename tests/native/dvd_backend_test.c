#include "dolphin/dvd.h"
#include "dolphin/ar.h"
#include "jaudio_NES/dvdthread.h"
#include "pc_disc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Exercise real pc_disc -> pc_dvd -> DVD task -> ARAM code. The image contains
 * only a generated filesystem and test bytes, no game assets or executable. */
enum { AUDIO_SIZE = 0x8060, IMAGE_SIZE = 0xc000, DATA_OFFSET = 0x2000 };
int g_pc_verbose;
static u8 audio[AUDIO_SIZE], output[AUDIO_SIZE];
static int regional, fallbacks;
static const char* regional_path;
static unsigned callbacks;
static u32 owner;

/* Only language activation is supplied by the test. Validation of complete PAL
 * packs and SafeBoot remains covered by language_test and production code. */
int pc_language_audio_override_enabled(void) { return regional; }
const char* pc_language_audio_rom_path(void) { return regional_path; }
void pc_language_audio_fallback_to_usa(void) { regional = 0; ++fallbacks; }
static void finished(u32 value) { owner = value; ++callbacks; }

static void write_file(const char* path, const void* bytes, size_t size) {
    FILE* f = fopen(path, "wb");
    assert(f && fwrite(bytes, 1, size, f) == size && fclose(f) == 0);
}

static void be32(u8* p, u32 value) {
    p[0] = value >> 24; p[1] = value >> 16; p[2] = value >> 8; p[3] = value;
}

static void read_and_load(void) {
    DVDFileInfo info;
    memset(&info, 0xa5, sizeof(info));
    assert(DVDOpen("/audiorom.img", &info));
    assert(info.length == AUDIO_SIZE && !info.callback && !info.cb.userData);
    assert(DVDReadPrio(&info, output, AUDIO_SIZE, 0, 2) == AUDIO_SIZE);
    assert(memcmp(output, audio, AUDIO_SIZE) == 0);
    assert(DVDClose(&info));

    u32 status = UINT32_MAX;
    unsigned old_callbacks = callbacks;
    DVDT_LoadtoARAM(0x1234, "/audiorom.img", 0x4000, 0, 0, &status, finished);
    assert(!status);
    pc_dvd_process_all_tasks();
    assert(status == AUDIO_SIZE && callbacks == old_callbacks + 1 && owner == 0x1234);
    memset(output, 0, sizeof(output));
    DVDT_ARAMtoDRAM(0x1235, (uintptr_t)output, 0x4000, AUDIO_SIZE, &status, finished);
    pc_dvd_process_all_tasks();
    assert(status == AUDIO_SIZE && callbacks == old_callbacks + 2 && owner == 0x1235);
    assert(memcmp(output, audio, AUDIO_SIZE) == 0);
}

int main(void) {
    char temporary[] = "/tmp/acgc-dvd-XXXXXX";
    char* original = getcwd(NULL, 0);
    assert(original && mkdtemp(temporary) && !chdir(temporary));
    for (unsigned i = 0; i < AUDIO_SIZE; ++i) audio[i] = (u8)(i * 73 + 19);
    assert(!mkdir("assets", 0700));
    write_file("assets/audiorom.img", audio, AUDIO_SIZE);
    ARInit(NULL, 0); jac_dvdproc_init();
    read_and_load(); /* extracted file backend */

    u8* image = calloc(1, IMAGE_SIZE);
    assert(image);
    memcpy(image, "GAFE01", 6);
    be32(image + 0x1c, 0xc2339f3d);
    be32(image + 0x420, 0x800);
    be32(image + 0x424, 0x1000);
    image[0x1000] = 1; be32(image + 0x1008, 2);
    be32(image + 0x100c, 1); be32(image + 0x1010, DATA_OFFSET);
    be32(image + 0x1014, AUDIO_SIZE);
    memcpy(image + 0x1019, "audiorom.img", 13);
    memcpy(image + DATA_OFFSET, audio, AUDIO_SIZE);
    write_file("fixture.iso", image, IMAGE_SIZE);
    FILE* borrowed = fopen("fixture.iso", "rb");
    assert(borrowed); pc_disc_set_fd(fileno(borrowed));
    assert(pc_disc_init()); assert(!fclose(borrowed));
    read_and_load(); /* ISO through the borrowed Android-style descriptor */
    assert(!unlink("assets/audiorom.img"));

    write_file("regional.img", audio, AUDIO_SIZE);
    regional = 1; regional_path = "regional.img";
    read_and_load(); /* already validated regional file */
    assert(!unlink("regional.img"));
    read_and_load(); /* removed regional file falls back to the open USA disc */
    assert(!regional && fallbacks == 1);
    pc_disc_shutdown();

    u8* ciso = calloc(1, 0x8000 + IMAGE_SIZE);
    assert(ciso); memcpy(ciso, "CISO", 4);
    ciso[5] = 0x40; /* little-endian block size 0x4000 */
    memset(ciso + 8, 1, IMAGE_SIZE / 0x4000);
    memcpy(ciso + 0x8000, image, IMAGE_SIZE);
    write_file("fixture.ciso", ciso, 0x8000 + IMAGE_SIZE);
    pc_disc_set_path("fixture.ciso"); assert(pc_disc_init());
    read_and_load(); /* CISO crosses physical blocks through the real reader */
    pc_disc_shutdown();
    assert(!unlink("fixture.iso") && !unlink("fixture.ciso") && !rmdir("assets"));
    assert(!chdir(original) && !rmdir(temporary));
    free(original); free(image); free(ciso);
    return 0;
}
