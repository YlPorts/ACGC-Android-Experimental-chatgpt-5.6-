#ifndef PC_AUDIO_BANK_H
#define PC_AUDIO_BANK_H
#include "jaudio_NES/audiostruct.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct PcAudioBank PcAudioBank;
/* Decode immutable BE bank bytes into native tables. Offsets are always u32;
 * the resulting pointers may be anywhere in the process address space. */
PcAudioBank* pc_audio_bank_decode(const u8* bytes, size_t size,
                                const voiceinfo* description, const WaveMedia* media,
                                size_t wave0_size, size_t wave1_size);
const voiceinfo* pc_audio_bank_voice(const PcAudioBank* bank);
size_t pc_audio_bank_samples(const PcAudioBank* bank, smzwavetable** samples, size_t capacity);
void pc_audio_bank_free(PcAudioBank* bank);
#ifdef __cplusplus
}
#endif
#endif
