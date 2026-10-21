#include "pc_audio_bank.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static void put16(u8* p, u16 value) { p[0] = value >> 8; p[1] = value; }
static void put32(u8* p, u32 value) {
    p[0] = value >> 24; p[1] = value >> 16; p[2] = value >> 8; p[3] = value;
}
int main(void) {
    u8 storage[513] = {0}, before[512];
    u8* bytes = storage + 1; /* Deliberately unaligned wire representation. */
    voiceinfo description = {0};
    description.num_instruments = 2;
    description.num_drums = 2;
    description.num_sfx = 1;
    put32(bytes, 16); put32(bytes + 4, 24);
    put32(bytes + 8, 32); put32(bytes + 12, 32); /* shared instrument */
    put32(bytes + 16, 64); put32(bytes + 20, 64); /* shared drum */
    put32(bytes + 24, 96); put32(bytes + 28, 0x3f800000);
    bytes[34] = 0x7f; bytes[35] = 42;
    put32(bytes + 36, 160);
    put32(bytes + 48, 96); put32(bytes + 52, 0x3f000000);
    bytes[64] = 33; bytes[65] = 64;
    put32(bytes + 68, 96); put32(bytes + 72, 0x40000000); put32(bytes + 76, 160);
    put32(bytes + 96, 64); /* ADPCM, wave0, size=64 */
    put32(bytes + 100, 7); put32(bytes + 104, 112); put32(bytes + 108, 192);
    put32(bytes + 112, 4); put32(bytes + 116, 60); put32(bytes + 120, 1);
    put32(bytes + 124, 64);
    for (unsigned i = 0; i < 16; ++i) put16(bytes + 128 + i * 2, i * 3);
    put16(bytes + 160, (u16)-4); put16(bytes + 162, 13); /* SPECIAL4 continues */
    put16(bytes + 164, 20); put16(bytes + 166, 30000);
    put16(bytes + 168, (u16)-1); put16(bytes + 170, 0);
    put32(bytes + 192, 2); put32(bytes + 196, 1);
    for (unsigned i = 0; i < 16; ++i) put16(bytes + 200 + i * 2, i * 5);
    memcpy(before, bytes, 512);
    u8* samples = malloc(128);
    assert(samples);
    WaveMedia media = {0};
    media.wave0_p = samples;
    media.wave0_media = MEDIUM_RAM;
    PcAudioBank* bank = pc_audio_bank_decode(bytes, 512, &description, &media, 128, 0);
    assert(bank && memcmp(before, bytes, 512) == 0);
    const voiceinfo* voice = pc_audio_bank_voice(bank);
    assert(voice->instruments[0] == voice->instruments[1]);
    assert(voice->percussion[0] == voice->percussion[1]);
    assert(voice->instruments[0]->adsr_decay_idx == 42);
    wtstr tuned = voice->instruments[0]->normal_pitch_tuned_sample;
    assert(tuned.tuning == 0.5f && voice->effects[0].tuned_sample.tuning == 1.0f);
    assert(tuned.wavetable == voice->percussion[0]->tuned_sample.wavetable);
    assert(tuned.wavetable == voice->effects[0].tuned_sample.wavetable);
    assert(tuned.wavetable->sample == samples + 7);
    assert(tuned.wavetable->loop->predictor_state[15] == 45);
    assert(tuned.wavetable->book->codebook[15] == 75);
    assert(voice->instruments[0]->envelope[2].delay == -1);
    assert(voice->instruments[0]->envelope == voice->percussion[0]->envelope);
    pc_audio_bank_free(bank);
    assert(!pc_audio_bank_decode(bytes, 10, &description, &media, 128, 0));
    put32(bytes + 8, 0xffffffffu);
    assert(!pc_audio_bank_decode(bytes, 512, &description, &media, 128, 0));
    memcpy(bytes, before, 512);
    put32(bytes + 108, 0xfffffffc);
    assert(!pc_audio_bank_decode(bytes, 512, &description, &media, 128, 0));
    memcpy(bytes, before, 512);
    put32(bytes + 100, 120); /* sample extends beyond its wave bank */
    assert(!pc_audio_bank_decode(bytes, 512, &description, &media, 128, 0));
    memcpy(bytes, before, 512);
    put32(bytes + 104, 0); /* missing loop used by the synthesis engine */
    assert(!pc_audio_bank_decode(bytes, 512, &description, &media, 128, 0));
    memcpy(bytes, before, 512);
    put32(bytes + 196, 100); /* would overflow rspsim's real codebook buffer */
    assert(!pc_audio_bank_decode(bytes, 512, &description, &media, 128, 0));
    free(samples);
    return 0;
}
