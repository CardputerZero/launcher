#ifndef __MAIN__H__
#define __MAIN__H__

#include <sys/queue.h>
#include <pthread.h>
#ifdef __cplusplus
extern "C" {
#endif

struct key_item {
    uint32_t key_code;
    char sym_name[65];
    char utf8[9];
    int key_state;
    STAILQ_ENTRY(key_item) entries;
};
STAILQ_HEAD(keyboard_queue_t, key_item);
extern struct keyboard_queue_t keyboard_queue;
extern pthread_mutex_t keyboard_mutex;
void *keyboard_read_thread(void *argv) ;
#ifdef __cplusplus
}
#endif
#endif