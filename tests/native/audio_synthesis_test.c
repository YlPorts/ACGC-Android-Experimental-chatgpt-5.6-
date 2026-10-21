/* Decode console bank bytes, synthesize notes, execute the real RSP command
 * stream and emit PCM for comparison between ARM32 and AArch64. */
#include "pc_audio_bank.h"
#include "jaudio_NES/driver.h"
#include "jaudio_NES/rspsim.h"
#include "jaudio_NES/audiocommon.h"
#include "jaudio_NES/os.h"
#include "dolphin/ar.h"
#include <SDL.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CPU cache/interrupt and SDL mutex adapters for Linux component tests. */
BOOL OSDisableInterrupts(void) { return TRUE; }
BOOL OSRestoreInterrupts(BOOL enabled) { return enabled; }
void DCTouchRange(void* p, u32 n) { (void)p; (void)n; }
void DCStoreRange(void* p, u32 n) { (void)p; (void)n; }
void DCZeroRange(void* p, u32 n) { memset(p, 0, n); }
/* Synthetic disc's ARAM origin, as in the DVD/ARAM component fixtures. */
u32 GetNeosRomTop(void) { return 0x10000; }
/* Synchronous virtual-disc transport; real ARAM copies and audio queues. */
BOOL ARAMStartDMAmesg(u32 direction, uintptr_t dram, u32 aram, u32 size, s32 unused, OSMesgQueue* queue) {
    (void)unused;
    ARStartDMA(direction, dram, aram + GetNeosRomTop(), size);
    if (queue) Z_osSendMesg(queue, NULL, OS_MESG_NOBLOCK);
    return FALSE;
}
struct SDL_mutex { pthread_mutex_t value; };
SDL_mutex* SDL_CreateMutex(void) {
    SDL_mutex* mutex = malloc(sizeof(*mutex));
    assert(mutex && !pthread_mutex_init(&mutex->value, NULL)); return mutex;
}
int SDL_LockMutex(SDL_mutex* mutex) { return pthread_mutex_lock(&mutex->value); }
int SDL_UnlockMutex(SDL_mutex* mutex) { return pthread_mutex_unlock(&mutex->value); }
void SDL_DestroyMutex(SDL_mutex* mutex) { assert(!pthread_mutex_destroy(&mutex->value)); free(mutex); }

static void put32(u8* p, u32 n) { p[0] = n >> 24; p[1] = n >> 16; p[2] = n >> 8; p[3] = n; }
static void synthesize(int codec, FILE* output) {
    enum { SAMPLES = 160, FRAMES = 12 };
    u8 bytes[256] = {0};
    u8 input[2048] ATTRIBUTE_ALIGN(32);
    for (unsigned i = 0; i < sizeof(input); ++i)
        input[i] = codec == CODEC_S8 ? (u8)((i % 64) - 32) : (i % 9 ? 0x11 : 0x90);
    /* Wire instrument at 16, envelope at 48, sample at 64, loop at 96,
     * predictor book at 160. Nothing contains a serialized host pointer. */
    put32(bytes + 8, 16);
    bytes[18] = 0x7f; bytes[19] = 42;
    put32(bytes + 20, 48); put32(bytes + 32, 64); put32(bytes + 36, 0x3f800000);
    bytes[48] = 0xff; bytes[49] = 0xff; /* ADSR_HANG */
    put32(bytes + 64, ((u32)codec << 28) | sizeof(input));
    put32(bytes + 72, 96); put32(bytes + 76, codec == CODEC_ADPCM ? 160 : 0);
    put32(bytes + 96, 16); put32(bytes + 100, 1024); put32(bytes + 104, 1); put32(bytes + 108, 1024);
    put32(bytes + 160, 2); put32(bytes + 164, 1);
    voiceinfo description = {0}; description.num_instruments = 1;
    WaveMedia media = {0}; media.wave0_p = input; media.wave0_media = MEDIUM_RAM;
    PcAudioBank* bank = pc_audio_bank_decode(bytes, sizeof(bytes), &description, &media, sizeof(input), 0);
    assert(bank);
    channel channel = {0}; commonch common = {0}; synthparams params = {0};
    memset(&AG, 0, sizeof(AG));
    AG.channels = &channel; AG.common_channel = &common; AG.num_channels = 1;
    AG.audio_params.updates_per_frame = 1;
    common.enabled = 1; common.target_volume_left = common.target_volume_right = 0x800;
    common.frequency_fixed_point = 0x8000;
    common.tuned_sample = &pc_audio_bank_voice(bank)->instruments[0]->normal_pitch_tuned_sample;
    channel.driver_ch.synth_params = &params;
    int peak = 0; unsigned long long energy = 0;
    for (int frame = 0; frame < FRAMES; ++frame) {
        common.needs_init = frame == 0; channel.common_ch = common;
        s16 pcm[SAMPLES * 2] = {0}; Acmd commands[256];
        Acmd* end = Nas_DriveRsp(pcm, SAMPLES, commands, 0);
        assert(end > commands && end < commands + 256);
        assert(RspStart((u32*)commands, (s32)(end - commands)) == end - commands);
        for (unsigned i = 0; i < SAMPLES * 2; ++i) {
            int value = pcm[i]; if (value < 0) value = -value;
            if (value > peak) peak = value;
            energy += (unsigned)value;
        }
        assert(fwrite(pcm, sizeof(pcm), 1, output) == 1);
    }
    assert(peak > 0 && energy > 0 && !AG.audio_error_flags);
    assert(channel.driver_ch.sample_pos_integer_part < 1024); /* loop was exercised */
    printf("Synthesis codec=%d frames=%d peak=%d energy=%llu pos=%d pointers=%zu\n", codec, FRAMES, peak, energy,
        channel.driver_ch.sample_pos_integer_part, sizeof(void*));
    pc_audio_bank_free(bank);
}
int main(int argc, char** argv) {
    assert(argc == 2);
    FILE* output = fopen(argv[1], "wb"); assert(output);
    synthesize(CODEC_S8, output); synthesize(CODEC_ADPCM, output);
    assert(!fclose(output));
    return 0;
}
