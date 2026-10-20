#include "pc_address.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

enum { REFERENCES = 40000, THREADS = 4 };
static unsigned char* storage;
static uint32_t references[REFERENCES];

static void* worker(void* argument) {
    size_t thread = (size_t)argument;
    for (size_t i = thread; i < REFERENCES; i += THREADS) {
        uintptr_t pointer = (uintptr_t)(storage + i);
        references[i] = pc_address_pack(pointer);
        assert(pc_address_unpack(references[i]) == pointer);
        assert(pc_address_pack(pointer) == references[i]);
    }
    return NULL;
}

int main(void) {
    pthread_t threads[THREADS];
    storage = (unsigned char*)malloc(REFERENCES);
    assert(storage);
    assert(pc_address_pack(0) == 0 && pc_address_unpack(0) == 0);
    for (size_t i = 0; i < THREADS; ++i)
        assert(pthread_create(&threads[i], NULL, worker, (void*)i) == 0);
    for (size_t i = 0; i < THREADS; ++i) assert(pthread_join(threads[i], NULL) == 0);
    /* Early references must still work after multiple table growths. Odd and
       even addresses are covered; the old 8192-slot wrapping is not acceptable. */
    for (size_t i = 0; i < REFERENCES; ++i)
        assert(pc_address_unpack(references[i]) == (uintptr_t)(storage + i));
#if UINTPTR_MAX > UINT32_MAX
    uintptr_t a = UINT64_C(0x0000000176543210);
    uintptr_t b = UINT64_C(0x0000000276543210);
    assert(pc_address_pack(a) != pc_address_pack(b));
    assert(pc_address_unpack(pc_address_pack(a)) == a);
    assert(pc_address_unpack(pc_address_pack(b)) == b);
    uintptr_t output;
    assert(!pc_address_try_unpack(UINT32_C(0x08000000), &output));
    assert(!pc_address_try_unpack(UINT32_C(0xffffffff), &output));
#endif
    free(storage);
    return 0;
}
