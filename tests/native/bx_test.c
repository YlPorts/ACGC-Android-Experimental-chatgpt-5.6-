#include "pc_bx.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static u8* read_file(const char* name,size_t* size) {
    FILE* f=fopen(name,"rb"); assert(f);
    assert(fseek(f,0,SEEK_END)==0); long length=ftell(f); assert(length>0);
    rewind(f); u8* bytes=malloc((size_t)length); assert(bytes);
    assert(fread(bytes,1,(size_t)length,f)==(size_t)length); fclose(f); *size=length; return bytes;
}
int main(int argc,char** argv) {
    assert(argc==3);
    for(int is_wave=0;is_wave<=1;is_wave++) {
        size_t size; u8* bytes=read_file(argv[is_wave+1],&size);
        u8* before=malloc(size); assert(before); memcpy(before,bytes,size);
        PcBx* bx=pc_bx_decode(bytes,size,is_wave); assert(bx);
        if(is_wave) {
            Wsys_* w=pc_bx_wsys(bx); assert(w && w->globalID==0);
            assert(w->waveArcBank && w->waveArcBank->count==1);
            WaveArchive_* arc=w->waveArcBank->waveGroups[0];
            assert(arc && arc->waveCount==2 && !strcmp(arc->filePath,"nintendo_hi_0.aw"));
            assert(arc->waves[0]->length>0 && arc->waves[1]->srcAddress>0);
            assert(w->ctrlGroup && w->ctrlGroup->count==1);
            assert(w->ctrlGroup->scenes[0]->cdf->count==2);
        } else {
            Bank_* b=pc_bx_bank(bx); assert(b && b->mMagic==0x42414e4b);
            unsigned count=0;
            for(unsigned i=0;i<BANK_TEST_INST_COUNT;i++) {
                Inst_* inst=b->mInstruments[i]; if(!inst) continue; ++count;
                assert(inst->mKeyRegionCount>0 && inst->mKeyRegions[0]->mVelocityCount>0);
                assert(inst->mKeyRegions[0]->mVelocities[0]);
            }
            Perc_* perc=b->mPercs[BANK_TEST_PERC_OFFSET];
            assert(perc && perc->mMagic);
            for(unsigned i=0;i<128;i++) {
                PercKeymap_* key=perc->mKeyRegions[i];
                if(key) { ++count; assert(key->mVelocityCount>0 && key->mVelocities[0]); }
            }
            assert(count>0);
        }
        assert(!memcmp(before,bytes,size)); pc_bx_free(bx);
        assert(!pc_bx_decode(bytes,31,is_wave));
        /* Truncation and a pointer outside the containing file both fail. */
        assert(!pc_bx_decode(bytes,size/2,is_wave));
        unsigned slot=is_wave ? 16 : 36;
        memset(bytes+slot,0xff,4);
        assert(!pc_bx_decode(bytes,size,is_wave));
        free(before); free(bytes);
    }
    puts("IBNK/WSYS: original embedded banks decode without altering file bytes");
    return 0;
}
