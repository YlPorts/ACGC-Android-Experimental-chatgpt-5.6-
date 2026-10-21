#include "pc_guest_memory.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t address;
    size_t size;
    uint8_t* data;
    int swap;
} GuestRange;
/* Built on the loading thread before the game starts; immutable thereafter. */
static GuestRange* ranges;
static size_t range_count, range_capacity;

void pc_guest_map(uint32_t address, void* data, size_t size, int swap_type) {
    if (!data || !size || size > (uint64_t)UINT32_MAX + 1 - address) return;
    for (size_t i=0;i<range_count;i++) {
        if (ranges[i].address==address && ranges[i].size==size) {
            ranges[i]=(GuestRange){address,size,data,swap_type}; return;
        }
    }
    if (range_count==range_capacity) {
        size_t next=range_capacity ? range_capacity*2 : 256;
        GuestRange* p=realloc(ranges,next*sizeof(*p));
        if (!p) abort();
        ranges=p; range_capacity=next;
    }
    ranges[range_count++]=(GuestRange){address,size,data,swap_type};
}
static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
void pc_guest_map_dol_asset(const uint8_t h[256],uint32_t offset,void* data,size_t size,int swap) {
    for(unsigned i=0;i<18;i++) {
        uint32_t start=be32(h+i*4), address=be32(h+0x48+i*4), length=be32(h+0x90+i*4);
        if (offset>=start && offset-start<length && size<=length-(offset-start)) {
            uint64_t guest=(uint64_t)address+offset-start;
            if (guest<=UINT32_MAX) pc_guest_map((uint32_t)guest,data,size,swap);
            return;
        }
    }
}
static GuestRange* find_range(uint32_t address) {
    /* Prefer the most recent mapping when an asset is reloaded. */
    for(size_t i=range_count;i>0;i--) {
        GuestRange* r=&ranges[i-1];
        if(address>=r->address && (size_t)(address-r->address)<r->size) return r;
    }
    return NULL;
}
static size_t native_offset(const GuestRange* r,size_t offset) {
    if(r->swap==1) return offset^1; /* SWAP_U16 */
    if(r->swap==3) return offset^3; /* SWAP_U32 */
    if(r->swap==2 && offset%16<12) return offset^1; /* SWAP_VTX */
    return offset;
}
static int transfer(uint32_t address,void* buffer,size_t size,int write) {
    if(size>(uint64_t)UINT32_MAX+1-address) return 0;
    /* Validate the complete span before writing any byte. */
    for(size_t i=0;i<size;i++) {
        GuestRange* r=find_range(address+(uint32_t)i);
        if(!r || native_offset(r,address+(uint32_t)i-r->address)>=r->size) return 0;
    }
    uint8_t* bytes=buffer;
    for(size_t i=0;i<size;i++) {
        GuestRange* r=find_range(address+(uint32_t)i);
        uint8_t* p=r->data+native_offset(r,address+(uint32_t)i-r->address);
        if(write) *p=bytes[i]; else bytes[i]=*p;
    }
    return 1;
}
int pc_guest_read(uint32_t address,void* data,size_t size) { return transfer(address,data,size,0); }
int pc_guest_write(uint32_t address,const void* data,size_t size) { return transfer(address,(void*)data,size,1); }
