// monitor_test.c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "monitor.h"


static monitor_t m;

void* waiter(void* arg) {
    int id = *(int*)arg;
    printf("Thread %d waiting...\n", id);
    int rc = monitor_wait(&m);
    if (rc) printf("Thread %d wait error: %d\n", id, rc);
    else printf("Thread %d awoke!\n", id);
    return NULL;
}

void* waiter_null(void* arg) {
    (void)arg;
    int rc = monitor_wait(NULL);
    printf("waiter_null: monitor_wait(NULL) returned %d (should be -1)\n", rc);
    return NULL;
}

void test_basic() {
    printf("\n=== Basic Monitor Test ===\n");
    assert(monitor_init(&m) == 0);
    pthread_t t;
    int id = 1;
    pthread_create(&t, NULL, waiter, &id);
    sleep(1);
    printf("Main: signaling...\n");
    monitor_signal(&m);
    pthread_join(t, NULL);
    monitor_destroy(&m);
    printf("Basic test passed.\n");
}

void test_double_init_destroy() {
    printf("\n=== Double Init/Destroy Edge Case ===\n");
    monitor_t m2;
    assert(monitor_init(&m2) == 0);
    // Double init (should fail or be safe)
    int rc = monitor_init(&m2);
    printf("Double init returned: %d (should be 0 or error)\n", rc);
    monitor_destroy(&m2);
    // Double destroy (should not crash)
    monitor_destroy(&m2);
    printf("Double destroy did not crash.\n");
}

void test_null_pointer() {
    printf("\n=== NULL Pointer Edge Case ===\n");
    // These should not crash
    monitor_destroy(NULL);
    monitor_signal(NULL);
    monitor_reset(NULL);
    int rc = monitor_wait(NULL);
    printf("monitor_wait(NULL) returned: %d (should be -1)\n", rc);
    pthread_t t;
    pthread_create(&t, NULL, waiter_null, NULL);
    pthread_join(t, NULL);
}

void test_signal_before_wait() {
    printf("\n=== Signal Before Wait (Lost Wakeup Prevention) ===\n");
    assert(monitor_init(&m) == 0);
    monitor_signal(&m);
    pthread_t t;
    int id = 2;
    pthread_create(&t, NULL, waiter, &id);
    pthread_join(t, NULL);
    monitor_destroy(&m);
    printf("Signal-before-wait test passed.\n");
}

void test_multi_waiters() {
    printf("\n=== Multi-Waiter Test ===\n");
    assert(monitor_init(&m) == 0);
    pthread_t t1, t2;
    int id1 = 1, id2 = 2;
    pthread_create(&t1, NULL, waiter, &id1);
    pthread_create(&t2, NULL, waiter, &id2);
    sleep(1);
    printf("Main: signaling...\n");
    monitor_signal(&m); // wakes up one waiter
    monitor_signal(&m); // wakes up the other waiter
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    monitor_destroy(&m);
    printf("Multi-waiter test passed.\n");
}

void test_reset() {
    printf("\n=== Reset Test ===\n");
    assert(monitor_init(&m) == 0);
    pthread_t t;
    int id = 3;
    pthread_create(&t, NULL, waiter, &id);
    sleep(1);
    monitor_signal(&m);
    pthread_join(t, NULL);
    printf("Resetting monitor...\n");
    monitor_reset(&m);
    // Waiter should block again
    pthread_create(&t, NULL, waiter, &id);
    sleep(1);
    monitor_signal(&m);
    pthread_join(t, NULL);
    monitor_destroy(&m);
    printf("Reset test passed.\n");
}

void test_uninitialized_monitor() {
    printf("\n=== Uninitialized Monitor Edge Case ===\n");
    printf("SKIPPED: This test is undefined behavior and may hang or crash.\n");
    // Do not actually call monitor_wait or other functions on an uninitialized monitor!
}

void* stress_waiter(void* arg) {
    monitor_t* mon = (monitor_t*)arg;
    for (int i = 0; i < 100; ++i) {
        monitor_wait(mon);
    }
    return NULL;
}

void test_stress() {
    printf("\n=== Stress Test: Many Waiters/Signals ===\n");
    monitor_t mon;
    assert(monitor_init(&mon) == 0);
    pthread_t threads[16];
    for (int i = 0; i < 16; ++i)
        pthread_create(&threads[i], NULL, stress_waiter, &mon);
    for (int i = 0; i < 100; ++i)
        monitor_signal(&mon);
    for (int i = 0; i < 16; ++i)
        pthread_join(threads[i], NULL);
    monitor_destroy(&mon);
    printf("Stress test passed.\n");
}

int main() {
    test_basic();
    test_double_init_destroy();
    test_null_pointer();
    test_signal_before_wait();
    test_multi_waiters();
    test_reset();
    test_uninitialized_monitor();
    test_stress();
    printf("\nAll monitor tests completed.\n");
    return 0;
}
