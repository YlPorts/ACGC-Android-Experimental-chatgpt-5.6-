#include "JSystem/JKernel/JKRExpHeap.h"
#include "JSystem/JUtility/JUTConsole.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdlib>
/* Single-thread platform adapters; allocations, splitting, resizing,
 * coalescing and disposer traversal use the production JKR implementation. */
static int locks;
extern "C" void OSPanic(const char* file,int line,const char* format,...) {
    std::fprintf(stderr,"%s:%d: ",file,line);
    va_list args; va_start(args,format); std::vfprintf(stderr,format,args); va_end(args);
    std::abort();
}
extern "C" void OSInitMutex(OSMutex* mutex) { assert(mutex); }
extern "C" void OSLockMutex(OSMutex* mutex) { assert(mutex); ++locks; }
extern "C" void OSUnlockMutex(OSMutex* mutex) { assert(mutex && locks>0); --locks; }
void JUTReportConsole(const char* text) { std::fputs(text,stderr); }
void JUTWarningConsole(const char* text) { std::fputs(text,stderr); }
void JUTReportConsole_f(const char* format,...) {
    va_list args; va_start(args,format); std::vfprintf(stderr,format,args); va_end(args);
}
void JUTWarningConsole_f(const char* format,...) {
    va_list args; va_start(args,format); std::vfprintf(stderr,format,args); va_end(args);
}
int main() {
    alignas(32) static unsigned char arena[32768];
    alignas(JKRExpHeap) static unsigned char object[sizeof(JKRExpHeap)];
    JKRExpHeap* heap=new (object) JKRExpHeap(arena,sizeof(arena),nullptr,false);
    JKRHeap::sRootHeap=heap;
    const int alignments[]={4,8,16,32,64,-4,-16,-64};
    struct Allocation { unsigned char* data; unsigned size; unsigned char pattern; } slots[64]={};
    for(unsigned cycle=0;cycle<4;cycle++) {
        for(unsigned i=0;i<64;i++) {
            int alignment=alignments[i%8]; unsigned size=1+(i*19+cycle*7)%157;
            unsigned char* p=(unsigned char*)heap->alloc(size,alignment); assert(p);
            assert((uintptr_t)p%(alignment<0 ? -alignment : alignment)==0);
            auto block=JKRExpHeap::CMemBlock::getBlock(p);
            assert((uintptr_t)block%alignof(JKRExpHeap::CMemBlock)==0);
            assert(heap->getSize(p)>=(int)size);
            slots[i]={p,size,(unsigned char)(i+cycle+1)};
            std::memset(p,slots[i].pattern,size);
        }
        assert(heap->check());
        for(unsigned i=0;i<64;i++) {
            Allocation& a=slots[i];
            for(unsigned j=0;j<a.size;j++) assert(a.data[j]==a.pattern);
            if(i%3==0) {
                heap->resize(a.data,a.size+17);
                for(unsigned j=0;j<a.size;j++) assert(a.data[j]==a.pattern);
            }
        }
        for(unsigned parity=0;parity<2;parity++)
            for(unsigned i=parity;i<64;i+=2) heap->free(slots[i].data);
        assert(heap->check());
        assert(heap->getFreeSize()==(int)(sizeof(arena)-sizeof(JKRExpHeap::CMemBlock)));
    }
    assert(locks==0);
    return 0;
}
