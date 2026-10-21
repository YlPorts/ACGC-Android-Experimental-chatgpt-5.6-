#include "dolphin/os/OSMessage.h"
#include "dolphin/os/OSAlarm.h"
#include "dolphin/os/OSMutex.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    OSMessageQueue queue;
    OSMessage messages[3], received = NULL;
    unsigned char data[3];
    memset(&queue, 0xa5, sizeof(queue));
    OSInitMessageQueue(&queue, messages, 3);
    assert(queue.msgArray == messages && queue.msgCount == 3);
    assert(queue.firstIndex == 0 && queue.usedCount == 0);
    assert(!queue.queueSend.head && !queue.queueSend.tail);
    assert(!queue.queueReceive.head && !queue.queueReceive.tail);
    assert(OSSendMessage(&queue, &data[0], OS_MESSAGE_NOBLOCK));
    assert(queue.usedCount == 1);
    assert(OSSendMessage(&queue, &data[1], OS_MESSAGE_NOBLOCK));
    assert(OSJamMessage(&queue, &data[2], OS_MESSAGE_NOBLOCK));
    assert(queue.usedCount == 3 && !OSSendMessage(&queue, NULL, 0));
    assert(OSReceiveMessage(&queue, &received, 0) && received == &data[2]);
    assert(OSReceiveMessage(&queue, &received, 0) && received == &data[0]);
    assert(OSReceiveMessage(&queue, &received, 0) && received == &data[1]);
    assert(!OSReceiveMessage(&queue, &received, 0) && queue.usedCount == 0);

    struct { OSContext value; unsigned char guard[320]; } context;
    memset(&context, 0xa5, sizeof(context));
    OSClearContext(&context.value);
    for (size_t i = 0; i < sizeof(context.guard); ++i) assert(context.guard[i] == 0xa5);

    struct { OSAlarm value; unsigned char guard[64]; } alarm;
    memset(&alarm, 0xa5, sizeof(alarm));
    OSCreateAlarm(&alarm.value);
    assert(!alarm.value.handler && !alarm.value.prev && !alarm.value.next);
    for (size_t i = 0; i < sizeof(alarm.guard); ++i) assert(alarm.guard[i] == 0xa5);

    OSThread* thread = OSGetCurrentThread();
    assert(thread && !thread->stackBase && !thread->stackEnd);
    assert(thread->priority == 0);
    OSMutex mutex;
    memset(&mutex, 0xa5, sizeof(mutex));
    OSInitMutex(&mutex);
    assert(!mutex.queue.head && !mutex.queue.tail && !mutex.thread && !mutex.count);
    assert(OSTryLockMutex(&mutex));
    assert(mutex.count == 1 && mutex.thread == thread);
    OSUnlockMutex(&mutex);
    assert(!mutex.count && !mutex.thread);
    OSLockMutex(&mutex);
    assert(mutex.count == 1 && mutex.thread == thread);
    OSUnlockMutex(&mutex);
    return 0;
}
