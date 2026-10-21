#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
void pc_fixnes_init(uint8_t*,int);
uint16_t* pc_fixnes_frame(void);
void pc_fixnes_set_input(uint8_t);
void pc_fixnes_sync_wram(uint8_t*);
void pc_fixnes_write_wram(unsigned,const uint8_t*,unsigned);
void pc_fixnes_cleanup(void);
static unsigned audio_calls,restore_calls;
/* Headless platform adapters. CPU/PPU/APU, mapper, input and the game's
 * fixNES wrapper below are the actual production sources. */
void pc_gx_restore_after_nes(void) { ++restore_calls; }
void AIInitDMA(uintptr_t address,uint32_t size) {
    assert(address && size && size%4==0);
    const int16_t* samples=(const int16_t*)address;
    volatile int16_t first=samples[0],last=samples[size/2-1];
    (void)first; (void)last; ++audio_calls;
}
int main(void) {
    /* Self-authored NROM fixture: compute 5+7, then repeatedly read the
     * controller's A button. No commercial ROM or game data is used. */
    const uint8_t program[]={
        0x78,                 /* SEI */
        0xa9,0x05,0x18,0x69,0x07,0x85,0x02, /* LDA #5; CLC; ADC #7; STA $02 */
        0xa9,0x01,0x8d,0x16,0x40, /* LDA #1; STA $4016 */
        0xa9,0x00,0x8d,0x16,0x40, /* LDA #0; STA $4016 */
        0xad,0x16,0x40,0x29,0x01,0x85,0x03, /* LDA $4016; AND #1; STA $03 */
        0x4c,0x08,0x80,        /* JMP controller loop at $8008 */
    };
    unsigned size=16+0x4000+0x2000;
    uint8_t* rom=calloc(1,size); assert(rom);
    memcpy(rom,"NES\x1a",4); rom[4]=1; rom[5]=1;
    memcpy(rom+16,program,sizeof(program));
    rom[16+0x3ffc]=0x00; rom[16+0x3ffd]=0x80;
    pc_fixnes_init(rom,size);
    pc_fixnes_set_input(1);
    for(unsigned i=0;i<3;i++) assert(pc_fixnes_frame());
    uint8_t ram[0x800]; pc_fixnes_sync_wram(ram);
    assert(ram[2]==12 && ram[3]==1 && audio_calls>0);
    pc_fixnes_set_input(0); pc_fixnes_frame(); pc_fixnes_sync_wram(ram);
    assert(ram[3]==0);
    uint8_t update[]={0x12,0x34,0x56};
    pc_fixnes_write_wram(0x100,update,sizeof(update)); pc_fixnes_sync_wram(ram);
    assert(!memcmp(ram+0x100,update,sizeof(update)));
    pc_fixnes_cleanup(); assert(restore_calls==1);
    free(rom); return 0;
}
