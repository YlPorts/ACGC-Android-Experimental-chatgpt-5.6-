#include "pc_address.h"

#if UINTPTR_MAX > UINT32_MAX
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#define REFERENCE_BASE UINT32_C(0xf0000000)
#define REFERENCE_LIMIT UINT32_C(0x0fffffff)

typedef struct {
    uintptr_t address;
    uint32_t index;
} AddressSlot;

static atomic_flag registry_lock = ATOMIC_FLAG_INIT;
static AddressSlot* slots;
static uintptr_t* addresses;
static size_t capacity;
static uint32_t count;

static void lock_registry(void) {
    while (atomic_flag_test_and_set_explicit(&registry_lock, memory_order_acquire)) {}
}

static void unlock_registry(void) {
    atomic_flag_clear_explicit(&registry_lock, memory_order_release);
}

static size_t hash_address(uintptr_t address) {
    uint64_t value = (uint64_t)address;
    value ^= value >> 33;
    value *= UINT64_C(0xff51afd7ed558ccd);
    value ^= value >> 33;
    return (size_t)value;
}

static void grow_registry(void) {
    size_t next = capacity ? capacity * 2 : 4096;
    AddressSlot* new_slots = (AddressSlot*)calloc(next, sizeof(*new_slots));
    uintptr_t* new_addresses = (uintptr_t*)realloc(addresses, next * sizeof(*addresses));
    if (!new_slots || !new_addresses) {
        fputs("[Address] Unable to allocate native address registry\n", stderr);
        abort();
    }
    addresses = new_addresses;
    for (size_t i = 0; i < capacity; ++i) {
        if (slots[i].index) {
            size_t slot = hash_address(slots[i].address) & (next - 1);
            while (new_slots[slot].index) slot = (slot + 1) & (next - 1);
            new_slots[slot] = slots[i];
        }
    }
    free(slots);
    slots = new_slots;
    capacity = next;
}

uint32_t pc_address_pack(uintptr_t address) {
    if (!address) return 0;
    lock_registry();
    if (!capacity || (size_t)(count + 1) > capacity / 2) grow_registry();
    size_t slot = hash_address(address) & (capacity - 1);
    while (slots[slot].index && slots[slot].address != address)
        slot = (slot + 1) & (capacity - 1);
    if (!slots[slot].index) {
        if (count == REFERENCE_LIMIT) {
            fputs("[Address] Native reference space exhausted\n", stderr);
            abort();
        }
        slots[slot].address = address;
        slots[slot].index = ++count;
        addresses[count] = address;
    }
    uint32_t reference = REFERENCE_BASE | slots[slot].index;
    unlock_registry();
    return reference;
}

int pc_address_try_unpack(uint32_t reference, uintptr_t* address) {
    if (!reference) {
        *address = 0;
        return 1;
    }
    if ((reference & REFERENCE_BASE) != REFERENCE_BASE) return 0;
    uint32_t index = reference & REFERENCE_LIMIT;
    lock_registry();
    int valid = index && index <= count;
    if (valid) *address = addresses[index];
    unlock_registry();
    return valid;
}

uintptr_t pc_address_unpack(uint32_t reference) {
    uintptr_t address;
    if (!pc_address_try_unpack(reference, &address)) {
        fprintf(stderr, "[Address] Invalid native reference: 0x%08x\n", reference);
        abort();
    }
    return address;
}

#else
uint32_t pc_address_pack(uintptr_t address) { return (uint32_t)address; }
uintptr_t pc_address_unpack(uint32_t reference) { return (uintptr_t)reference; }
int pc_address_try_unpack(uint32_t reference, uintptr_t* address) {
    *address = (uintptr_t)reference;
    return 1;
}
#endif
