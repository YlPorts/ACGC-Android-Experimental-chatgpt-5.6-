#include "pc_audio_bank.h"
#include "jaudio_NES/audiocommon.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Chunk {
    struct Chunk* next;
    size_t used, capacity;
    max_align_t alignment;
    unsigned char data[];
} Chunk;

typedef struct Object {
    struct Object* next;
    u32 offset;
    unsigned type;
    void* pointer;
} Object;

struct PcAudioBank {
    const u8* bytes;
    size_t size;
    Chunk* chunks;
    Object* objects;
    voiceinfo voice;
    WaveMedia media;
    size_t wave_size[2];
    smzwavetable* samples[128];
    size_t sample_count;
    int failed;
};

enum { OBJECT_WAVE, OBJECT_LOOP, OBJECT_BOOK, OBJECT_ENV, OBJECT_INST, OBJECT_DRUM };

static int contains(PcAudioBank* bank, u32 offset, size_t size) {
    if (offset > bank->size || size > bank->size - offset) {
        bank->failed = 1;
        return 0;
    }
    return 1;
}

static u16 read16(PcAudioBank* bank, u32 offset) {
    if (!contains(bank, offset, 2)) return 0;
    return ((u16)bank->bytes[offset] << 8) | bank->bytes[offset + 1];
}

static u32 read32(PcAudioBank* bank, u32 offset) {
    if (!contains(bank, offset, 4)) return 0;
    const u8* p = bank->bytes + offset;
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static void* allocate(PcAudioBank* bank, size_t size) {
    if (bank->failed) return NULL;
    if (size > SIZE_MAX - 15) { bank->failed = 1; return NULL; }
    size = (size + 15) & ~(size_t)15;
    Chunk* chunk = bank->chunks;
    if (!chunk || size > chunk->capacity - chunk->used) {
        size_t capacity = size > 4096 ? size : 4096;
        if (capacity > SIZE_MAX - sizeof(Chunk)) { bank->failed = 1; return NULL; }
        chunk = (Chunk*)malloc(sizeof(Chunk) + capacity);
        if (!chunk) { bank->failed = 1; return NULL; }
        chunk->next = bank->chunks;
        chunk->used = 0;
        chunk->capacity = capacity;
        bank->chunks = chunk;
    }
    void* pointer = chunk->data + chunk->used;
    chunk->used += size;
    memset(pointer, 0, size);
    return pointer;
}

static void* find_object(PcAudioBank* bank, u32 offset, unsigned type) {
    for (Object* object = bank->objects; object; object = object->next)
        if (object->offset == offset && object->type == type) return object->pointer;
    return NULL;
}

static void* new_object(PcAudioBank* bank, u32 offset, unsigned type, size_t size) {
    Object* object = (Object*)allocate(bank, sizeof(Object));
    void* pointer = allocate(bank, size);
    if (!object || !pointer) return NULL;
    object->offset = offset;
    object->type = type;
    object->pointer = pointer;
    object->next = bank->objects;
    bank->objects = object;
    return pointer;
}

static envdat* decode_envelope(PcAudioBank* bank, u32 offset) {
    if (!offset) return NULL;
    envdat* result = (envdat*)find_object(bank, offset, OBJECT_ENV);
    if (result) return result;
    size_t count;
    for (count = 0; count < 66; ++count) {
        if (!contains(bank, offset, (count + 1) * 4)) return NULL;
        s16 delay = (s16)read16(bank, offset + count * 4);
        if (delay == 0 || delay == -1 || delay == -2 || delay == -3) break;
    }
    if (count == 66) { bank->failed = 1; return NULL; }
    ++count;
    result = (envdat*)new_object(bank, offset, OBJECT_ENV, count * sizeof(envdat));
    if (!result) return NULL;
    for (size_t i = 0; i < count; ++i) {
        result[i].delay = (s16)read16(bank, offset + i * 4);
        result[i].value = (s16)read16(bank, offset + i * 4 + 2);
    }
    return result;
}

static adpcmloop* decode_loop(PcAudioBank* bank, u32 offset) {
    if (!offset) return NULL;
    adpcmloop* result = (adpcmloop*)find_object(bank, offset, OBJECT_LOOP);
    if (result) return result;
    if (!contains(bank, offset, 16)) return NULL;
    u32 count = read32(bank, offset + 8);
    if (count && !contains(bank, offset, 48)) return NULL;
    result = (adpcmloop*)new_object(bank, offset, OBJECT_LOOP, sizeof(adpcmloop));
    if (!result) return NULL;
    result->loop_start = read32(bank, offset);
    result->loop_end = read32(bank, offset + 4);
    result->count = count;
    result->sample_end = read32(bank, offset + 12);
    if (count) for (unsigned i = 0; i < 16; ++i)
        result->predictor_state[i] = (s16)read16(bank, offset + 16 + i * 2);
    return result;
}

static adpcmbook* decode_book(PcAudioBank* bank, u32 offset) {
    if (!offset) return NULL;
    adpcmbook* result = (adpcmbook*)find_object(bank, offset, OBJECT_BOOK);
    if (result) return result;
    if (!contains(bank, offset, 8)) return NULL;
    u32 order = read32(bank, offset), predictors = read32(bank, offset + 4);
    if ((s32)order <= 0 || (s32)predictors <= 0 || predictors > (SIZE_MAX - 8) / 16 / order) {
        bank->failed = 1;
        return NULL;
    }
    size_t coefficients = (size_t)order * predictors * 8;
    /* rspsim's actual codebook buffer has 8 predictors x 16 coefficients. */
    if (coefficients > 128) { bank->failed = 1; return NULL; }
    size_t size = 8 + coefficients * 2;
    if (!contains(bank, offset, size)) return NULL;
    result = (adpcmbook*)new_object(bank, offset, OBJECT_BOOK, size);
    if (!result) return NULL;
    result->order = order;
    result->n_predictors = predictors;
    for (size_t i = 0; i < coefficients; ++i)
        result->codebook[i] = (s16)read16(bank, offset + 8 + i * 2);
    return result;
}

static smzwavetable* decode_wave(PcAudioBank* bank, u32 offset) {
    if (!offset) return NULL;
    smzwavetable* result = (smzwavetable*)find_object(bank, offset, OBJECT_WAVE);
    if (result) return result;
    if (!contains(bank, offset, 16)) return NULL;
    result = (smzwavetable*)new_object(bank, offset, OBJECT_WAVE, sizeof(smzwavetable));
    if (!result) return NULL;
    u32 flags = read32(bank, offset);
    memcpy(result, &flags, sizeof(flags));
    uintptr_t sample = read32(bank, offset + 4);
    if (result->size) {
        result->loop = decode_loop(bank, read32(bank, offset + 8));
        result->book = decode_book(bank, read32(bank, offset + 12));
        if (!result->loop || ((result->codec == CODEC_ADPCM || result->codec == CODEC_SMALL_ADPCM) && !result->book)) {
            bank->failed = 1; return NULL;
        }
        if (result->medium == MEDIUM_RAM || result->medium == MEDIUM_DISK) {
            unsigned index = result->medium == MEDIUM_DISK;
            uintptr_t base = (uintptr_t)(index ? bank->media.wave1_p : bank->media.wave0_p);
            if (sample > bank->wave_size[index] || result->size > bank->wave_size[index] - sample || base > UINTPTR_MAX - sample) {
                bank->failed = 1; return NULL;
            }
        }
        if (result->medium == MEDIUM_RAM) {
            sample += (uintptr_t)bank->media.wave0_p;
            result->medium = bank->media.wave0_media;
        } else if (result->medium == MEDIUM_DISK) {
            sample += (uintptr_t)bank->media.wave1_p;
            result->medium = bank->media.wave1_media;
        }
        result->is_relocated = TRUE;
        if (result->bit26 && result->medium != MEDIUM_RAM && bank->sample_count < 128)
            bank->samples[bank->sample_count++] = result;
    }
    result->sample = (u8*)sample;
    return result;
}

static wtstr decode_tuned(PcAudioBank* bank, u32 offset) {
    wtstr result = {0};
    if (!contains(bank, offset, 8)) return result;
    result.wavetable = decode_wave(bank, read32(bank, offset));
    u32 tuning = read32(bank, offset + 4);
    memcpy(&result.tuning, &tuning, sizeof(tuning));
    return result;
}

static voicetable* decode_instrument(PcAudioBank* bank, u32 offset) {
    if (!offset) return NULL;
    voicetable* result = (voicetable*)find_object(bank, offset, OBJECT_INST);
    if (result) return result;
    if (!contains(bank, offset, 32)) return NULL;
    result = (voicetable*)new_object(bank, offset, OBJECT_INST, sizeof(voicetable));
    if (!result) return NULL;
    result->is_relocated = TRUE;
    result->normal_range_low = bank->bytes[offset + 1];
    result->normal_range_high = bank->bytes[offset + 2];
    result->adsr_decay_idx = bank->bytes[offset + 3];
    result->envelope = decode_envelope(bank, read32(bank, offset + 4));
    if (result->normal_range_low) result->low_pitch_tuned_sample = decode_tuned(bank, offset + 8);
    result->normal_pitch_tuned_sample = decode_tuned(bank, offset + 16);
    if (result->normal_range_high != 0x7f) result->high_pitch_tuned_sample = decode_tuned(bank, offset + 24);
    return result;
}

static perctable* decode_drum(PcAudioBank* bank, u32 offset) {
    if (!offset) return NULL;
    perctable* result = (perctable*)find_object(bank, offset, OBJECT_DRUM);
    if (result) return result;
    if (!contains(bank, offset, 16)) return NULL;
    result = (perctable*)new_object(bank, offset, OBJECT_DRUM, sizeof(perctable));
    if (!result) return NULL;
    result->adsr_decay_idx = bank->bytes[offset];
    result->pan = bank->bytes[offset + 1];
    result->is_relocated = TRUE;
    result->tuned_sample = decode_tuned(bank, offset + 4);
    result->envelope = decode_envelope(bank, read32(bank, offset + 12));
    return result;
}

PcAudioBank* pc_audio_bank_decode(const u8* bytes, size_t size,
                                const voiceinfo* description, const WaveMedia* media,
                                size_t wave0_size, size_t wave1_size) {
    if (!bytes || !description || !media || size > UINT32_MAX) return NULL;
    PcAudioBank* bank = (PcAudioBank*)calloc(1, sizeof(PcAudioBank));
    if (!bank) return NULL;
    bank->bytes = bytes;
    bank->size = size;
    bank->voice = *description;
    bank->media = *media;
    bank->wave_size[0] = wave0_size; bank->wave_size[1] = wave1_size;
    unsigned instruments = description->num_instruments;
    if (instruments > 126) instruments = 126;
    bank->voice.instruments = allocate(bank, instruments * sizeof(voicetable*));
    bank->voice.percussion = allocate(bank, description->num_drums * sizeof(perctable*));
    bank->voice.effects = allocate(bank, description->num_sfx * sizeof(percvoicetable));
    if (bank->failed || !contains(bank, 0, (2 + instruments) * 4)) goto failure;
    u32 drums = read32(bank, 0), effects = read32(bank, 4);
    if (drums && description->num_drums) {
        if (!contains(bank, drums, description->num_drums * 4)) goto failure;
        for (unsigned i = 0; i < description->num_drums; ++i)
            bank->voice.percussion[i] = decode_drum(bank, read32(bank, drums + i * 4));
    }
    if (effects && description->num_sfx) {
        if (!contains(bank, effects, description->num_sfx * 8)) goto failure;
        for (unsigned i = 0; i < description->num_sfx; ++i)
            bank->voice.effects[i].tuned_sample = decode_tuned(bank, effects + i * 8);
    }
    for (unsigned i = 0; i < instruments; ++i)
        bank->voice.instruments[i] = decode_instrument(bank, read32(bank, 8 + i * 4));
    if (!bank->failed) return bank;
failure:
    pc_audio_bank_free(bank);
    return NULL;
}

const voiceinfo* pc_audio_bank_voice(const PcAudioBank* bank) { return &bank->voice; }

size_t pc_audio_bank_samples(const PcAudioBank* bank, smzwavetable** samples, size_t capacity) {
    size_t count = bank->sample_count < capacity ? bank->sample_count : capacity;
    memcpy(samples, bank->samples, count * sizeof(*samples));
    return count;
}

void pc_audio_bank_free(PcAudioBank* bank) {
    if (!bank) return;
    Chunk* chunk = bank->chunks;
    while (chunk) { Chunk* next = chunk->next; free(chunk); chunk = next; }
    free(bank);
}
