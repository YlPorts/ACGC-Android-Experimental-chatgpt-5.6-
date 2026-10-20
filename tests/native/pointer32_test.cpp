#include "pc_pointer32.hpp"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include <cstddef>
#include <cassert>
#include <cstdint>

int main() {
    static_assert(sizeof(JKRArchive::SDIFileEntry) == 20, "RARC entries retain the original wire stride");
    static_assert(offsetof(JKRArchive::SDIFileEntry, mData) == 16, "RARC reserved runtime slot stays at 16");
    JKRExpHeap::CMemBlock blocks[2] = {};
    assert(alignof(JKRExpHeap::CMemBlock) == 16);
    assert(sizeof(blocks[0]) == (sizeof(void*) == 8 ? 32 : 16));
    assert(JKRExpHeap::CMemBlock::getBlock(blocks[0].getContent()) == &blocks[0]);
    int value = 42;
    JKRArchive::SDIFileEntry entries[2] = {};
    entries[0].mData = &value;
    assert(static_cast<void*>(entries[0].mData) == &value);
    assert(reinterpret_cast<char*>(&entries[1])-reinterpret_cast<char*>(&entries[0]) == 20);
    PcPointer32<void> reference = {0};
    assert(reference == nullptr);
    reference = &value;
    assert(reference != nullptr);
    assert(static_cast<int*>(reference) == &value);
    assert(*static_cast<int*>(reference) == 42);
    PcPointer32<void> copy = reference;
    assert(static_cast<uintptr_t>(copy) == reinterpret_cast<uintptr_t>(&value));
    reference = nullptr;
    assert(!reference);
    assert(static_cast<int*>(copy) == &value);
    return 0;
}
