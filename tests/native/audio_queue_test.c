#include "jaudio_NES/os.h"
#include "jaudio_NES/sub_sys.h"
#include <SDL.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

/* POSIX synchronization adapter for the Linux component tests. Android links
 * the original SDL implementation. Queue and audio consumers are production. */
struct SDL_mutex { pthread_mutex_t value; };
SDL_mutex* SDL_CreateMutex(void) {
    SDL_mutex* mutex = malloc(sizeof(*mutex));
    assert(mutex && !pthread_mutex_init(&mutex->value, NULL));
    return mutex;
}
int SDL_LockMutex(SDL_mutex* mutex) { return pthread_mutex_lock(&mutex->value); }
int SDL_UnlockMutex(SDL_mutex* mutex) { return pthread_mutex_unlock(&mutex->value); }
void SDL_DestroyMutex(SDL_mutex* mutex) { assert(!pthread_mutex_destroy(&mutex->value)); free(mutex); }
void SDL_Delay(Uint32 milliseconds) {
    struct timespec delay = { milliseconds / 1000, (milliseconds % 1000) * 1000000L };
    nanosleep(&delay, NULL);
}
void pc_audio_mq_init(void);
void pc_audio_mq_shutdown(void);

int main(void) {
    OSMesgQueue queue;
    OSMesg messages[4], received = NULL;
    unsigned char data;
    pc_audio_mq_init();
    Z_osCreateMesgQueue(&queue, messages, 4);
    AG.spec_change_mq_p = &queue;
    AG.spec_id = 3;
    /* The empty receive wrote eight bytes into the real consumer's four-byte
     * local on LP64, even before any sound or disc content was processed. */
    assert(Nap_CheckSpecChange() == 0);
    assert(!Z_osSendMesg(&queue, (OSMesg)(uintptr_t)3, OS_MESG_NOBLOCK));
    assert(Nap_CheckSpecChange() == 1);
    assert(!Z_osSendMesg(&queue, (OSMesg)(uintptr_t)7, OS_MESG_NOBLOCK));
    assert(Nap_CheckSpecChange() == -1);
    assert(!Z_osSendMesg(&queue, &data, OS_MESG_NOBLOCK));
    assert(!Z_osRecvMesg(&queue, &received, OS_MESG_NOBLOCK) && received == &data);
    struct { u32 word; u32 guard; } result = { 0, 0xa5b6c7d8 };
    assert(!Z_osSendMesg(&queue, (OSMesg)(uintptr_t)UINT32_C(0xf1234567), 0));
    assert(!Z_osRecvMesgU32(&queue, &result.word, 0));
    assert(result.word == UINT32_C(0xf1234567) && result.guard == UINT32_C(0xa5b6c7d8));
    assert(Z_osRecvMesgU32(&queue, &result.word, 0) == -1);
    assert(!result.word && result.guard == UINT32_C(0xa5b6c7d8));
    pc_audio_mq_shutdown();
    return 0;
}
