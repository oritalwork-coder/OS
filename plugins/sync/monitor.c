#include "monitor.h"
#include <errno.h>

int monitor_init(monitor_t *m) {
    if (!m) return -1;
    int rc;
    rc = pthread_mutex_init(&m->mutex, NULL);
    if (rc) return -1;
    rc = pthread_cond_init(&m->condition, NULL);
    if (rc) {
        pthread_mutex_destroy(&m->mutex);
        return -1;
    }
    m->signaled = 0;
    return 0;
}

void monitor_signal(monitor_t *m) {
    if (!m) return;
    pthread_mutex_lock(&m->mutex);
    m->signaled = 1;
    pthread_cond_signal(&m->condition);
    pthread_mutex_unlock(&m->mutex);
}

void monitor_reset(monitor_t *m) {
    if (!m) return;
    pthread_mutex_lock(&m->mutex);
    m->signaled = 0;
    pthread_mutex_unlock(&m->mutex);
}

int monitor_wait(monitor_t *m) {
    if (!m) return -1;
    int rc = pthread_mutex_lock(&m->mutex);
    if (rc) return -1;
    while (!m->signaled) {
        rc = pthread_cond_wait(&m->condition, &m->mutex);
        if (rc) break;
    }
    pthread_mutex_unlock(&m->mutex);
    if (rc == 0)
        return 0;
    else
        return -1;
}

void monitor_destroy(monitor_t *m) {
    if (!m) return;
    pthread_cond_destroy(&m->condition);
    pthread_mutex_destroy(&m->mutex);
}