#include "pc_guest_memory.h"
#include <assert.h>
#include <string.h>
static void be32(unsigned char* p,uint32_t word) {
    p[0]=word>>24; p[1]=word>>16; p[2]=word>>8; p[3]=word;
}
int main(void) {
    unsigned char header[256]={0},data[16]={0x44,0x33,0x22,0x11},copy[4];
    be32(header,0x100); be32(header+0x48,0x80003100); be32(header+0x90,0x100);
    pc_guest_map_dol_asset(header,0x140,data,sizeof(data),3);
    assert(pc_guest_read(0x80003140,copy,4));
    assert(!memcmp(copy,"\x11\x22\x33\x44",4));
    assert(pc_guest_write(0x80003141,"\xaa\xbb",2));
    assert(data[2]==0xaa && data[1]==0xbb);
    assert(!pc_guest_write(0x8000314f,"\xff\xff",2));
    assert(data[12]==0); /* failed span does not partly modify the asset */
    assert(!pc_guest_read(0x80000000,copy,4));
    assert(!pc_guest_read(UINT32_MAX,copy,4));
    pc_guest_map(0x81000000,data,sizeof(data),1);
    assert(pc_guest_read(0x81000000,copy,4));
    assert(copy[0]==data[1] && copy[3]==data[2]);
    return 0;
}
