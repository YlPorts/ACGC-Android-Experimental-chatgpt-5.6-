#ifdef TARGET_PC
#include "JSystem/JFramework/JFWSystem.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "dolphin/dvd.h"
#include "dolphin/os.h"
#include <stdlib.h>

// Keep CPU-only heap startup independently testable with the real OS backend.
int JFWSystem::CSetUpParam::maxStdHeaps = 2;
u32 JFWSystem::CSetUpParam::sysHeapSize = 0x400000;
u32 JFWSystem::CSetUpParam::exConsoleBufferSize = 0x24F8;
JKRHeap* JFWSystem::rootHeap;
JKRHeap* JFWSystem::systemHeap;

void JFWSystem::firstInit() {
    JUT_ASSERT(rootHeap == 0);
    OSInit();
    DVDInit();
    rootHeap = JKRExpHeap::createRoot(CSetUpParam::maxStdHeaps, false);
    const s32 available = rootHeap->getFreeSize();
    u32 systemHeapSize = CSetUpParam::sysHeapSize;
    if (systemHeapSize == SYS_HEAP_SIZE_AVAILABLE) {
        // createRoot has already consumed OS descriptors, alignment and its own
        // native header/block. In AArch64 these exceed JW_Init's old 0xD0 budget.
        // The initial root has one contiguous, 16-byte-aligned payload, so its
        // free size is the exact capacity for the child heap allocation.
        systemHeapSize = (u32)available & ~0xFu;
    }
    systemHeap = JKRExpHeap::create(systemHeapSize, rootHeap, false);
    OSReport("[PC/Heap] root=%p available=%d system-request=%u system=%p\n",
             (void*)rootHeap, available, systemHeapSize, (void*)systemHeap);
    if (!systemHeap) {
        // Do not continue initialization with a null system heap; retain the
        // allocation error in the log instead of faulting at a later virtual call.
        OSPanic(__FILE__, __LINE__, "System heap creation failed: requested %u, available %d",
                systemHeapSize, available);
        abort();
    }
}

#endif // TARGET_PC
