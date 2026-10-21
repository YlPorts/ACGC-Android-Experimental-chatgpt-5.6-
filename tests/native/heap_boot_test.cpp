#include "JSystem/JFramework/JFWSystem.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "dolphin/os/OSArena.h"
#include <SDL.h>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <time.h>

/* The SDL clock and diagnostic output are adapted to POSIX. Arena initialization,
 * heap creation, mutexes, allocation and disposal are production implementations. */
extern "C" {
int g_pc_verbose = 1;
int g_pc_time_override = -1, g_pc_min_override = -1, g_pc_sec_override = -1;
int g_pc_date_month, g_pc_date_day, g_pc_date_year;
Uint64 SDL_GetPerformanceCounter() {
    struct timespec now;
    assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
    return (Uint64)now.tv_sec * 1000000000 + now.tv_nsec;
}
Uint64 SDL_GetPerformanceFrequency() { return 1000000000; }
}

void JUTReportConsole(const char* text) { std::fputs(text, stderr); }
void JUTWarningConsole(const char* text) { std::fputs(text, stderr); }
void JUTReportConsole_f(const char* format, ...) {
    va_list args; va_start(args, format); std::vfprintf(stderr, format, args); va_end(args);
}
void JUTWarningConsole_f(const char* format, ...) {
    va_list args; va_start(args, format); std::vfprintf(stderr, format, args); va_end(args);
}

int main(int argc, char** argv) {
    OSInit();
#ifdef PC_NATIVE_64
    if (argc == 2 && !std::strcmp(argv[1], "--offset-arena")) {
        // malloc alignment and the arena's current bounds can vary. Require
        // the native-size policy to account for OS rounding as well as headers.
        OSSetArenaLo((u8*)OSGetArenaLo() + 16);
        OSSetArenaHi((u8*)OSGetArenaHi() - 16);
    }
#endif
    const u32 arenaSize = (u32)((u8*)OSGetArenaHi() - (u8*)OSGetArenaLo());
    const bool legacy = argc == 2 && !std::strcmp(argv[1], "--legacy-budget");
    const bool fixed = argc == 2 && !std::strcmp(argv[1], "--fixed-budget");
    u32 budget = arenaSize - 0xD0; // JW_Init's original ARM32 calculation.
#ifdef PC_NATIVE_64
    if (!legacy) budget = JFWSystem::SYS_HEAP_SIZE_AVAILABLE;
#endif
    if (fixed) budget = 0x400000; // Explicit JFWSystem callers retain their size.
    JFWSystem::setMaxStdHeap(1);
    JFWSystem::setSysHeapSize(budget);
    std::fprintf(stderr, "boot: pointer=%zu arena=%u heap-header=%zu block=%zu request=%u\n",
                 sizeof(void*), arenaSize, sizeof(JKRExpHeap), sizeof(JKRExpHeap::CMemBlock), budget);
    JFWSystem::firstInit();
    auto root = JFWSystem::rootHeap;
    auto system = JFWSystem::systemHeap;
    std::fprintf(stderr, "boot: root=%p system=%p root-free=%d\n",
                 (void*)root, (void*)system, root ? root->getFreeSize() : -1);
    assert(root && system); // RC3 ARM64 failed here, before JFWSystem::init's virtual call.
    assert(root->check() && system->check());
    assert(JKRHeap::sSystemHeap == system && JKRHeap::sCurrentHeap == system);
    if (fixed) assert(system->getHeapSize() + ((sizeof(JKRExpHeap) + 15) & ~15u) == budget);
    const int freeBefore = system->getFreeSize();
    void* console = system->alloc(JFWSystem::CSetUpParam::exConsoleBufferSize, 4);
    assert(console && JKRHeap::findFromRoot(console) == system);
    std::memset(console, 0xa5, JFWSystem::CSetUpParam::exConsoleBufferSize);
    // Exercise nested heap ownership/coalescing without disc or GPU data.
    JKRExpHeap* child = JKRExpHeap::create(0x380000, system, false);
    assert(child && child->check());
    void* data = child->alloc(65537, 32);
    assert(data && (uintptr_t)data % 32 == 0 && JKRHeap::findFromRoot(data) == child);
    std::memset(data, 0x3c, 65537);
    for (unsigned i = 0; i < 65537; ++i) assert(((u8*)data)[i] == 0x3c);
    for (u32 i = 0; i < JFWSystem::CSetUpParam::exConsoleBufferSize; ++i)
        assert(((u8*)console)[i] == 0xa5);
    child->free(data);
    delete child;
    // JW_Init2 reserves the game arena from the system heap with this formula.
    // Keep the console allocation alive to check that the large block cannot
    // overlap it. No claim is made about loading archives or running the game.
    const u32 gameArenaSize = system->getFreeSize() - 0x10000;
    auto gameArena = (u8*)system->alloc(gameArenaSize, 32);
    assert(gameArena && (uintptr_t)gameArena % 32 == 0);
    assert(JKRHeap::findFromRoot(gameArena) == system);
    std::memset(gameArena, 0x7e, gameArenaSize);
    for (u32 i = 0; i < gameArenaSize; ++i) assert(gameArena[i] == 0x7e);
    for (u32 i = 0; i < JFWSystem::CSetUpParam::exConsoleBufferSize; ++i)
        assert(((u8*)console)[i] == 0xa5);
    system->free(gameArena);
    system->free(console);
    assert(root->check() && system->check() && system->getFreeSize() == freeBefore);
    std::fprintf(stderr, "boot: console and game heap allocation/free passed\n");
    return 0;
}
