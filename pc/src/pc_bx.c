#include "pc_bx.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct BxObject {
    struct BxObject* next;
    u32 offset;
    unsigned kind;
    max_align_t alignment;
    u8 data[];
} BxObject;
struct PcBx {
    const u8* bytes;
    size_t size;
    int failed;
    BxObject* objects;
    Bank_* bank;
    Wsys_* wsys;
};
enum { K_BANK, K_INST, K_KEY, K_PKEY, K_VMAP, K_OSC, K_ENV, K_RAND,
       K_SENSE, K_PERC, K_VOICE, K_WSYS, K_WIN, K_ARC, K_WAVE, K_GROUP,
       K_SCENE, K_CTRL, K_ID, K_OPAQUE };
static int has(PcBx* b, size_t o, size_t n) {
    if (o > b->size || n > b->size - o) { b->failed = 1; return 0; }
    return 1;
}
static u16 r16(PcBx* b, size_t o) {
    if (!has(b, o, 2)) return 0;
    return ((u16)b->bytes[o] << 8) | b->bytes[o+1];
}
static u32 r32(PcBx* b, size_t o) {
    if (!has(b, o, 4)) return 0;
    return ((u32)b->bytes[o] << 24) | ((u32)b->bytes[o+1] << 16) |
           ((u32)b->bytes[o+2] << 8) | b->bytes[o+3];
}
static float rf(PcBx* b, size_t o) {
    u32 word = r32(b, o); float value; memcpy(&value, &word, 4); return value;
}
static void* object(PcBx* b, u32 o, unsigned kind, size_t wire, size_t native, int* fresh) {
    *fresh = 0;
    if (!o || b->failed || !has(b, o, wire)) return NULL;
    for (BxObject* p=b->objects; p; p=p->next)
        if (p->offset == o && p->kind == kind) return p->data;
    if (native > SIZE_MAX - sizeof(BxObject)) { b->failed=1; return NULL; }
    BxObject* p=calloc(1, sizeof(*p)+native);
    if (!p) { b->failed=1; return NULL; }
    p->offset=o; p->kind=kind; p->next=b->objects; b->objects=p; *fresh=1;
    return p->data;
}
#define NEW(type, kind, wire, native) int fresh; type* v=object(b,o,kind,wire,native,&fresh); if (!v || !fresh) return v
static int count_ok(PcBx* b, size_t o, u32 count, size_t stride) {
    if (count > b->size / stride) { b->failed=1; return 0; }
    return has(b, o, (size_t)count * stride);
}
static s16* envelope(PcBx* b, u32 o) {
    if (!o) return NULL;
    size_t n=0;
    /* A terminal command (14/15) or a loop command (13) ends the table. */
    while (has(b, (size_t)o+n*6, 6)) {
        u16 command=r16(b,(size_t)o+n*6); ++n;
        if (command>=13) break;
    }
    if (b->failed) return NULL;
    NEW(s16,K_ENV,n*6,n*6);
    for (size_t i=0;i<n*3;i++) v[i]=(s16)r16(b,(size_t)o+i*2);
    if (v[(n-1)*3]==13 && (u16)v[(n-1)*3+2]>=n) b->failed=1;
    return v;
}
static Osc_* oscillator(PcBx* b,u32 o) {
    NEW(Osc_,K_OSC,24,sizeof(*v));
    v->mode=b->bytes[o]; v->rate=rf(b,o+4);
    v->attackVecOffset=envelope(b,r32(b,o+8)); v->releaseVecOffset=envelope(b,r32(b,o+12));
    v->width=rf(b,o+16); v->vertex=rf(b,o+20); return v;
}
static Rand_* random_effect(PcBx* b,u32 o) {
    NEW(Rand_,K_RAND,16,sizeof(*v));
    v->id=b->bytes[o]; v->value=rf(b,o+4); v->range=rf(b,o+8);
    memcpy(v->_0C,b->bytes+o+12,4); return v;
}
static Sense_* sensor(PcBx* b,u32 o) {
    NEW(Sense_,K_SENSE,12,sizeof(*v));
    v->id=b->bytes[o]; v->type=b->bytes[o+1]; v->threshold=b->bytes[o+2];
    v->min=rf(b,o+4); v->max=rf(b,o+8); return v;
}
static Vmap_* velocity(PcBx* b,u32 o) {
    NEW(Vmap_,K_VMAP,16,sizeof(*v));
    v->mBaseVelocity=b->bytes[o]; v->mWsysID=(s16)r16(b,o+4); v->mWaveID=(s16)r16(b,o+6);
    v->mVolume=rf(b,o+8); v->mPitch=rf(b,o+12); return v;
}
static InstKeymap_* keymap(PcBx* b,u32 o) {
    if (!o || !has(b,o,8)) return NULL;
    u32 n=r32(b,(size_t)o+4);
    if (!count_ok(b,(size_t)o+8,n,4)) return NULL;
    NEW(InstKeymap_,K_KEY,8+(size_t)n*4,offsetof(InstKeymap_,mVelocities)+(size_t)n*sizeof(Vmap_*));
    v->mBaseKey=b->bytes[o]; v->mVelocityCount=n;
    for (u32 i=0;i<n;i++) v->mVelocities[i]=velocity(b,r32(b,(size_t)o+8+i*4));
    return v;
}
static Inst_* instrument(PcBx* b,u32 o) {
    if (!o || !has(b,o,44)) return NULL;
    if (!o) return NULL;
    u32 n=r32(b,(size_t)o+40);
    if (!count_ok(b,(size_t)o+44,n,4)) return NULL;
    NEW(Inst_,K_INST,44+(size_t)n*4,offsetof(Inst_,mKeyRegions)+(size_t)n*sizeof(InstKeymap_*));
    v->mMagic=r32(b,o); v->mFlag=r32(b,o+4); v->mFreqMultiplier=rf(b,o+8); v->mGainMultiplier=rf(b,o+12);
    for (u32 i=0;i<2;i++) {
        v->mOscillators[i]=oscillator(b,r32(b,o+16+i*4));
        v->mEffects[i]=random_effect(b,r32(b,o+24+i*4));
        v->mSensors[i]=sensor(b,r32(b,o+32+i*4));
    }
    v->mKeyRegionCount=n;
    for (u32 i=0;i<n;i++) v->mKeyRegions[i]=keymap(b,r32(b,(size_t)o+44+i*4));
    return v;
}
/* Unidentified voice/percussion records are opaque byte tables. Their
 * pointers still resolve safely without changing the original file. */
static void* opaque(PcBx* b,u32 o) {
    if (!o) return NULL;
    NEW(u8,K_OPAQUE,1,b->size-o);
    memcpy(v,b->bytes+o,b->size-o); return v;
}
static Voice_* voice(PcBx* b,u32 o) {
    if (!o || !has(b,o,12)) return NULL;
    if (!o) return NULL;
    u32 n=r32(b,(size_t)o+8);
    if (!count_ok(b,(size_t)o+12,n,4)) return NULL;
    NEW(Voice_,K_VOICE,12+(size_t)n*4,offsetof(Voice_,_0C)+(size_t)n*sizeof(void*));
    memcpy(v->_00,b->bytes+o,8); v->size=n;
    for(u32 i=0;i<n;i++) v->_0C[i]=opaque(b,r32(b,(size_t)o+12+i*4));
    return v;
}
static PercKeymap_* percussion_key(PcBx* b,u32 o) {
    if (!o || !has(b,o,20)) return NULL;
    if (!o) return NULL;
    u32 n=r32(b,(size_t)o+16);
    if (!count_ok(b,(size_t)o+20,n,4)) return NULL;
    NEW(PercKeymap_,K_PKEY,20+(size_t)n*4,offsetof(PercKeymap_,mVelocities)+(size_t)n*sizeof(Vmap_*));
    v->mPitch=rf(b,o); v->mVolume=rf(b,o+4); v->_08=random_effect(b,r32(b,o+8)); v->_0C=random_effect(b,r32(b,o+12));
    v->mVelocityCount=n;
    for(u32 i=0;i<n;i++) v->mVelocities[i]=velocity(b,r32(b,(size_t)o+20+i*4));
    return v;
}
static Perc_* percussion(PcBx* b,u32 o) {
    NEW(Perc_,K_PERC,0x408,sizeof(*v));
    v->mMagic=r32(b,o); memcpy(v->_04,b->bytes+o+4,sizeof(v->_04));
    for(u32 i=0;i<128;i++) { v->mKeyRegions[i]=percussion_key(b,r32(b,o+0x88+i*4)); v->_288[i]=b->bytes[o+0x288+i]; v->_308[i]=r16(b,o+0x308+i*2); }
    return v;
}
static Bank_* bank(PcBx* b,u32 o) {
    NEW(Bank_,K_BANK,4+BANK_INST_COUNT*4,sizeof(*v)); v->mMagic=r32(b,o);
    if(v->mMagic!=0x42414e4b) { b->failed=1; return NULL; }
    for(u32 i=0;i<BANK_INST_COUNT;i++) {
        u32 p=r32(b,o+4+i*4);
        if(i<BANK_TEST_INST_COUNT) v->mInstruments[i]=instrument(b,p);
        else if(i<BANK_TEST_PERC_OFFSET) v->mVoices[i]=voice(b,p);
        else v->mPercs[i]=percussion(b,p);
    }
    return v;
}
static Wave_* wave(PcBx* b,u32 o) {
    NEW(Wave_,K_WAVE,40,sizeof(*v));
    v->_00=b->bytes[o]; v->compBlockIdx=b->bytes[o+1]; v->key=b->bytes[o+2]; v->_04=rf(b,o+4);
    v->srcAddress=r32(b,o+8); v->length=r32(b,o+12); v->isLooping=r32(b,o+16); v->loopAddress=r32(b,o+20);
    v->loopStartPosition=r32(b,o+24); v->_1C=r32(b,o+28); v->loopYN1=r16(b,o+32); v->loopYN2=r16(b,o+34);
    return v;
}
static WaveArchive_* archive(PcBx* b,u32 o) {
    if (!o || !has(b,o,116)) return NULL;
    if(!o) return NULL;
    u32 n=r32(b,(size_t)o+0x70);
    if(!count_ok(b,(size_t)o+0x74,n,4)) return NULL;
    NEW(WaveArchive_,K_ARC,0x74+(size_t)n*4,offsetof(WaveArchive_,waves)+(size_t)n*sizeof(Wave_*));
    memcpy(v->filePath,b->bytes+o,64);
    if(!memchr(v->filePath,0,64)) { b->failed=1; return NULL; }
    v->waveCount=n;
    for(u32 i=0;i<n;i++) v->waves[i]=wave(b,r32(b,(size_t)o+0x74+i*4));
    return v;
}
static WaveArchiveBank_* wave_bank(PcBx* b,u32 o) {
    if (!o || !has(b,o,8)) return NULL;
    if(!o) return NULL; u32 n=r32(b,(size_t)o+4);
    if(!count_ok(b,(size_t)o+8,n,4)) return NULL;
    NEW(WaveArchiveBank_,K_WIN,8+(size_t)n*4,offsetof(WaveArchiveBank_,waveGroups)+(size_t)n*sizeof(WaveArchive_*));
    v->magic=r32(b,o); v->count=n;
    if(v->magic!=0x57494e46) b->failed=1;
    for(u32 i=0;i<n;i++) v->waveGroups[i]=archive(b,r32(b,(size_t)o+8+i*4));
    return v;
}
static WaveID_* wave_id(PcBx* b,u32 o) {
    NEW(WaveID_,K_ID,0x38,sizeof(*v)); v->id=r32(b,o); return v;
}
static Ctrl_* control(PcBx* b,u32 o) {
    if (!o || !has(b,o,8)) return NULL;
    if(!o) return NULL; u32 n=r32(b,(size_t)o+4);
    if(!count_ok(b,(size_t)o+8,n,4)) return NULL;
    NEW(Ctrl_,K_CTRL,8+(size_t)n*4,offsetof(Ctrl_,waveIDs)+(size_t)n*sizeof(WaveID_*));
    v->magic=r32(b,o); v->count=n;
    for(u32 i=0;i<n;i++) v->waveIDs[i]=wave_id(b,r32(b,(size_t)o+8+i*4));
    return v;
}
static SCNE_* scene(PcBx* b,u32 o) {
    if (!o || !has(b,o,24)) return NULL;
    if(!o) return NULL; u32 n=r32(b,(size_t)o+8);
    if(!count_ok(b,(size_t)o+24,n,4)) return NULL;
    NEW(SCNE_,K_SCENE,24+(size_t)n*4,offsetof(SCNE_,_18)+(size_t)n*4);
    v->magic=r32(b,o); v->_04=r32(b,o+4); v->_08=n;
    v->cdf=control(b,r32(b,o+12)); v->cex=control(b,r32(b,o+16)); v->cst=control(b,r32(b,o+20));
    for(u32 i=0;i<n;i++) v->_18[i]=r32(b,(size_t)o+24+i*4);
    return v;
}
static CtrlGroup_* group(PcBx* b,u32 o) {
    if (!o || !has(b,o,12)) return NULL;
    if(!o) return NULL; u32 n=r32(b,(size_t)o+8);
    if(!count_ok(b,(size_t)o+12,n,4)) return NULL;
    NEW(CtrlGroup_,K_GROUP,12+(size_t)n*4,offsetof(CtrlGroup_,scenes)+(size_t)n*sizeof(SCNE_*));
    v->magic=r32(b,o); v->count=n;
    if(v->magic!=0x57424354) b->failed=1;
    for(u32 i=0;i<n;i++) v->scenes[i]=scene(b,r32(b,(size_t)o+12+i*4));
    return v;
}
PcBx* pc_bx_decode(const u8* bytes,size_t size,int wave_system) {
    if(!bytes || size<32) return NULL;
    PcBx* b=calloc(1,sizeof(*b)); if(!b) return NULL; b->bytes=bytes; b->size=size;
    u32 declared=r32(b,4);
    if(declared<32 || declared>size) b->failed=1; else b->size=declared;
    if(wave_system) {
        if(r32(b,0)!=0x57535953) b->failed=1;
        int fresh; b->wsys=object(b,1,K_WSYS,0,sizeof(Wsys_),&fresh);
        if(b->wsys) {
            b->wsys->magic=r32(b,0); b->wsys->size=declared; b->wsys->globalID=r32(b,8);
            b->wsys->waveArcBank=wave_bank(b,r32(b,16)); b->wsys->ctrlGroup=group(b,r32(b,20));
            if(!b->wsys->waveArcBank || !b->wsys->ctrlGroup || b->wsys->waveArcBank->count!=b->wsys->ctrlGroup->count) b->failed=1;
        }
    } else {
        if(r32(b,0)!=0x49424e4b) b->failed=1;
        b->bank=bank(b,32);
    }
    if(b->failed || (!b->bank && !b->wsys)) { pc_bx_free(b); return NULL; }
    return b;
}
Bank_* pc_bx_bank(PcBx* b) { return b ? b->bank : NULL; }
Wsys_* pc_bx_wsys(PcBx* b) { return b ? b->wsys : NULL; }
void pc_bx_free(PcBx* b) {
    if(!b) return;
    while(b->objects) { BxObject* p=b->objects; b->objects=p->next; free(p); }
    free(b);
}
