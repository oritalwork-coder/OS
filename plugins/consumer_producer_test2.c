#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include <signal.h>
#include <limits.h>
#include <stdbool.h>
#include "consumer_producer.h"

// Test configuration
#define MAX_TEST_THREADS 8
#define STRESS_TEST_ITEMS 1000
#define STRESS_TEST_DURATION 5  // seconds

// Test results tracking
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
} test_results_t;

// Thread test data structures
typedef struct {
    consumer_producer_t* queue;
    int thread_id;
    int items_to_produce;
    int items_produced;
    int start_value;  // For generating unique strings
} producer_data_t;

typedef struct {
    consumer_producer_t* queue;
    int thread_id;
    int items_to_consume;
    int items_consumed;
    char** consumed_items;  // For verification
} consumer_data_t;

// Global test results
test_results_t g_results = {0, 0, 0};

// Helper functions
void print_test_header(const char* test_name) {
    printf("\n=== %s ===\n", test_name);
}

void print_test_result(const char* test_name, int passed) {
    g_results.total_tests++;
    if (passed) {
        g_results.passed_tests++;
        printf("✅ %s: PASSED\n", test_name);
    } else {
        g_results.failed_tests++;
        printf("❌ %s: FAILED\n", test_name);
    }
}

char* create_test_string(int value) {
    char* str = malloc(32);
    snprintf(str, 32, "test_item_%d", value);
    return str;
}

// =============================================================================
// EDGE CASE TESTS  
// =============================================================================

int test_null_pointer_handling() {
    print_test_header("NULL Pointer Edge Cases");
    
    consumer_producer_t queue;
    consumer_producer_init(&queue, 5);
    
    // Test 1: NULL queue pointer to functions
    printf("  Testing NULL queue pointer handling...\n");
    
    int result = consumer_producer_init(NULL, 5);
    if (result == 0) {  // Should fail
        print_test_result("NULL queue to init", 0);
        consumer_producer_destroy(&queue);
        return 0;
    }
    
    result = consumer_producer_put(NULL, "test");
    if (result == 0) {  // Should fail
        print_test_result("NULL queue to put", 0);
        consumer_producer_destroy(&queue);
        return 0;
    }
    
    char* item = consumer_producer_get(NULL);
    if (item != NULL) {  // Should return NULL
        print_test_result("NULL queue to get", 0);
        free(item);
        consumer_producer_destroy(&queue);
        return 0;
    }
    
    result = consumer_producer_wait_finished(NULL);
    if (result != -1) {  // Should return -1 for error
        print_test_result("NULL queue to wait_finished", 0);
        consumer_producer_destroy(&queue);
        return 0;
    }
    
    // Test 2: NULL item to put
    printf("  Testing NULL item handling...\n");
    result = consumer_producer_put(&queue, NULL);
    if (result == 0) {  // Should fail
        print_test_result("NULL item to put", 0);
        consumer_producer_destroy(&queue);
        return 0;
    }
    
    // Test 3: Calling destroy on NULL
    printf("  Testing destroy with NULL...\n");
    consumer_producer_destroy(NULL);  // Should not crash
    
    // Test 4: Calling signal_finished on NULL
    printf("  Testing signal_finished with NULL...\n");
    consumer_producer_signal_finished(NULL);  // Should not crash
    
    consumer_producer_destroy(&queue);
    print_test_result("NULL Pointer Edge Cases", 1);
    return 1;
}

int test_invalid_parameters() {
    print_test_header("Invalid Parameter Edge Cases");
    
    consumer_producer_t queue;
    
    // Test 1: Invalid capacity values
    printf("  Testing invalid capacity values...\n");
    
    int result = consumer_producer_init(&queue, 0);
    if (result == 0) {
        
        consumer_producer_destroy(&queue);
        print_test_result("Zero capacity handling", 0);
        return 0;
    }
    
    result = consumer_producer_init(&queue, -1);
    if (result == 0) {
        consumer_producer_destroy(&queue);
        print_test_result("Negative capacity handling", 0);
        return 0;
    }

    
    
    // Test 2: Large but valid capacity (should succeed)
    printf("  Testing large valid capacity...\n");
    result = consumer_producer_init(&queue, 1000);
    if (result != 0) {
        print_test_result("Large valid capacity handling", 0);
        return 0;
    }
    consumer_producer_destroy(&queue);
    
    // Test 2: Double initialization
    printf("  Testing double initialization...\n");
    result = consumer_producer_init(&queue, 5);
    if (result != 0) {
        print_test_result("First initialization failed", 0);
        return 0;
    }
    
    // Try to initialize again - should either fail or handle gracefully
    result = consumer_producer_init(&queue, 10);
    // Note: This behavior depends on implementation - it might succeed or fail
    // The important thing is that it doesn't crash
    
    consumer_producer_destroy(&queue);
    
    // Test 3: Operations on destroyed queue
    printf("  Testing operations on destroyed queue...\n");
    consumer_producer_destroy(&queue);  // Destroy it
    
    // These operations should fail gracefully, not crash
    char* item = consumer_producer_get(&queue);
    if (item != NULL) {
        free(item);
        print_test_result("Get from destroyed queue", 0);
        return 0;
    }
    
    result = consumer_producer_put(&queue, "test");
    if (result == 0) {
        print_test_result("Put to destroyed queue", 0);
        return 0;
    }
    
    print_test_result("Invalid Parameter Edge Cases", 1);
    return 1;
}

// =============================================================================
// TIMEOUT/BLOCKING BEHAVIOR TESTS
// =============================================================================

// Global flag for timeout tests
volatile int timeout_test_flag = 0;

void timeout_alarm_handler(int sig) {
    timeout_test_flag = 1;
}

typedef struct {
    consumer_producer_t* queue;
    int operation_completed;
    char* result_item;
} blocking_test_data_t;

void* blocking_consumer_thread(void* arg) {
    blocking_test_data_t* data = (blocking_test_data_t*)arg;
    
    printf("    Consumer thread: attempting to get from empty queue...\n");
    data->result_item = consumer_producer_get(data->queue);
    data->operation_completed = 1;
    printf("    Consumer thread: got item '%s'\n", 
           data->result_item ? data->result_item : "NULL");
    
    return NULL;
}

void* blocking_producer_thread(void* arg) {
    blocking_test_data_t* data = (blocking_test_data_t*)arg;
    
    printf("    Producer thread: attempting to put to full queue...\n");
    char* item = create_test_string(999);
    int result = consumer_producer_put(data->queue, item);
    
    if (result != 0) {
        free(item);
        printf("    Producer thread: put failed\n");
    } else {
        printf("    Producer thread: put succeeded\n");
    }
    
    data->operation_completed = 1;
    return NULL;
}

int test_blocking_behavior() {
    print_test_header("Blocking Behavior Verification");
    
    consumer_producer_t queue;
    if (consumer_producer_init(&queue, 2) != 0) {
        print_test_result("Blocking Test Setup", 0);
        return 0;
    }
    
    // Test 1: Consumer blocks on empty queue
    printf("  Testing consumer blocking on empty queue...\n");
    
    blocking_test_data_t consumer_data = {&queue, 0, NULL};
    pthread_t consumer_thread;
    
    pthread_create(&consumer_thread, NULL, blocking_consumer_thread, &consumer_data);
    
    // Give consumer time to start and block
    usleep(100000);  // 100ms
    
    if (consumer_data.operation_completed) {
        pthread_join(consumer_thread, NULL);
        if (consumer_data.result_item) free(consumer_data.result_item);
        consumer_producer_destroy(&queue);
        print_test_result("Consumer should block on empty queue", 0);
        return 0;
    }
    
    printf("   Consumer is properly blocked, now providing an item...\n");
    
    // Provide an item to unblock the consumer
    char* test_item = create_test_string(123);
    int res = consumer_producer_put(&queue, test_item);
    fprintf(stderr, "PUT RESULT: %d\n", res);
    
    pthread_join(consumer_thread, NULL);
    
    if (!consumer_data.operation_completed || !consumer_data.result_item) {
        if (consumer_data.result_item) free(consumer_data.result_item);
        consumer_producer_destroy(&queue);
        print_test_result("Consumer unblocking failed", 0);
        return 0;
    }
    
    if (strcmp(consumer_data.result_item, "test_item_123") != 0) {
        printf("    Wrong item received: %s\n", consumer_data.result_item);
        free(consumer_data.result_item);
        consumer_producer_destroy(&queue);
        print_test_result("Consumer received wrong item", 0);
        return 0;
    }
    
    free(consumer_data.result_item);
    printf("  Consumer blocking test passed\n");
    
    // Test 2: Producer blocks on full queue
    printf("  Testing producer blocking on full queue...\n");
    
    // Fill the queue to capacity
    for (int i = 0; i < 2; i++) {
        char* item = create_test_string(i);
        consumer_producer_put(&queue, item);
    }
    
    blocking_test_data_t producer_data = {&queue, 0, NULL};
    pthread_t producer_thread;
    
    pthread_create(&producer_thread, NULL, blocking_producer_thread, &producer_data);
    
    // Give producer time to start and block
    usleep(100000);  // 100ms
    
    if (producer_data.operation_completed) {
        pthread_join(producer_thread, NULL);
        consumer_producer_destroy(&queue);
        print_test_result("Producer should block on full queue", 0);
        return 0;
    }
    
    printf("    Producer is properly blocked, now making space...\n");
    
    // Remove an item to unblock the producer
    char* removed_item = consumer_producer_get(&queue);
    free(removed_item);
    
    pthread_join(producer_thread, NULL);
    
    if (!producer_data.operation_completed) {
        consumer_producer_destroy(&queue);
        print_test_result("Producer unblocking failed", 0);
        return 0;
    }
    
    printf("  Producer blocking test passed\n");
    fprintf(stderr, "YOU ARE HERE\n");
    // Clean up remaining items
    consumer_producer_signal_finished(&queue);
    while (true) {
        char* item = consumer_producer_get(&queue);
        fprintf(stderr, "GET RESULT: %s\n", item ? item : "NULL");
        if (item) {
            free(item);
        } else {
            break;
        }
    }
    
    consumer_producer_destroy(&queue);
    print_test_result("Blocking Behavior Verification", 1);
    return 1;
}

typedef struct {
    consumer_producer_t* queue;
    int* wait_flag;
} wait_test_data_t;

void* wait_thread_func(void* arg) {
    wait_test_data_t* data = (wait_test_data_t*) arg;
    consumer_producer_wait_finished(data->queue);
    *(data->wait_flag) = 1;
    return NULL;
}

int test_finished_signal_timing() {
    print_test_header("Finished Signal Timing Tests");

    consumer_producer_t queue;
    if (consumer_producer_init(&queue, 5) != 0) {
        print_test_result("Finished Signal Timing Setup", 0);
        return 0;
    }

    printf("  Testing blocking behavior before signal...\n");

    int wait_completed = 0;
    pthread_t waiter_thread;

    wait_test_data_t data = { .queue = &queue, .wait_flag = &wait_completed };
    pthread_create(&waiter_thread, NULL, wait_thread_func, &data);

    // Give the waiter thread time to block
    usleep(200000); // 200ms

    if (wait_completed != 0) {
        pthread_join(waiter_thread, NULL);
        consumer_producer_destroy(&queue);
        print_test_result("Should block before signal", 0);
        return 0;
    }

    printf("    Waiter thread is correctly blocked. Now sending signal...\n");

    // Now signal finished
    consumer_producer_signal_finished(&queue);

    pthread_join(waiter_thread, NULL);

    if (wait_completed == 0) {
        consumer_producer_destroy(&queue);
        print_test_result("Should unblock after signal", 0);
        return 0;
    }

    consumer_producer_destroy(&queue);
    print_test_result("Finished Signal Timing Tests", 1);
    return 1;
}


// =============================================================================
// BASIC TESTS
// =============================================================================

int test_init_destroy() {
    print_test_header("Basic Initialization and Destruction");
    
    consumer_producer_t queue;
    
    // Test 1: Normal initialization
    printf("  Testing normal initialization (capacity=5)...\n");
    int result = consumer_producer_init(&queue, 5);
    if (result != 0) {
        print_test_result("Normal initialization", 0);
        return 0;
    }
    
    // Verify initial state
    if (queue.capacity != 5 || queue.count != 0 || 
        queue.head != 0 || queue.tail != 0 || queue.items == NULL) {
        printf("    Initial state verification failed\n");
        consumer_producer_destroy(&queue);
        print_test_result("Initial state verification", 0);
        return 0;
    }
    
    consumer_producer_destroy(&queue);
    printf("  Basic init/destroy successful\n");
    
    // Test 2: Edge case - capacity of 1
    printf("  Testing edge case (capacity=1)...\n");
    result = consumer_producer_init(&queue, 1);
    if (result != 0) {
        print_test_result("Capacity=1 initialization", 0);
        return 0;
    }
    consumer_producer_destroy(&queue);
    
    // Test 3: Error case - invalid capacity
    printf("  Testing error case (capacity=0)...\n");
    result = consumer_producer_init(&queue, 0);
    if (result == 0) {  // Should fail
        consumer_producer_destroy(&queue);
        print_test_result("Invalid capacity handling", 0);
        return 0;
    }
    
    print_test_result("Basic Initialization and Destruction", 1);
    return 1;
}

int test_single_producer_consumer() {
    
    print_test_header("Single Producer/Consumer Operations");
    
    consumer_producer_t queue;
    if (consumer_producer_init(&queue, 3) != 0) {
        print_test_result("Single Producer/Consumer Setup", 0);
        return 0;
    }
    
    // Test 1: Single put/get
    printf("  Testing single put/get operation...\n");
    char* test_item = create_test_string(42);
    int put_result = consumer_producer_put(&queue, test_item);
    fprintf(stderr, "PUT RESULT: %d\n", put_result);

    if (put_result != 0) {
        free(test_item);
        consumer_producer_destroy(&queue);
        print_test_result("Single put operation", 0);
        return 0;
    }
    
    char* retrieved_item = consumer_producer_get(&queue);
    if (retrieved_item == NULL || strcmp(retrieved_item, "test_item_42") != 0) {
        printf("    Retrieved item mismatch: expected 'test_item_42', got '%s'\n", 
               retrieved_item ? retrieved_item : "NULL");
        free(retrieved_item);
        consumer_producer_destroy(&queue);
        print_test_result("Single get operation", 0);
        return 0;
    }
    
    free(retrieved_item);  // Consumer owns the memory now
    printf("  Single put/get successful\n");
    
    // Test 2: Multiple sequential operations
    printf("  Testing multiple sequential operations...\n");
    for (int i = 0; i < 5; i++) {
        char* item = create_test_string(i);
        
        if (consumer_producer_put(&queue, item) != 0) {
            free(item);
            consumer_producer_destroy(&queue);
            print_test_result("Multiple sequential puts", 0);
            return 0;
        }
        
        char* got_item = consumer_producer_get(&queue);
        char expected[32];
        snprintf(expected, 32, "test_item_%d", i);
        
        if (got_item == NULL || strcmp(got_item, expected) != 0) {
            printf("    Item %d mismatch: expected '%s', got '%s'\n", 
                   i, expected, got_item ? got_item : "NULL");
            free(got_item);
            consumer_producer_destroy(&queue);
            print_test_result("Multiple sequential operations", 0);
            return 0;
        }
        
        free(got_item);
    }
    
    consumer_producer_destroy(&queue);
    print_test_result("Single Producer/Consumer Operations", 1);
    return 1;
}

int test_queue_capacity_limits() {
    print_test_header("Queue Capacity and Limits");
    
    consumer_producer_t queue;
    if (consumer_producer_init(&queue, 2) != 0) {  // Small capacity for testing
        print_test_result("Capacity Limits Setup", 0);
        return 0;
    }
    
    printf("  Testing queue filling to capacity...\n");
    
    // Fill the queue to capacity
    for (int i = 0; i < 2; i++) {
        char* item = create_test_string(i);
        if (consumer_producer_put(&queue, item) != 0) {
            free(item);
            consumer_producer_destroy(&queue);
            print_test_result("Fill to capacity", 0);
            return 0;
        }
    }
    
    printf("  Queue filled to capacity successfully\n");
    
    // Empty the queue
    printf("  Testing queue emptying...\n");
    for (int i = 0; i < 2; i++) {
        char* item = consumer_producer_get(&queue);
        if (item == NULL) {
            consumer_producer_destroy(&queue);
            print_test_result("Empty queue", 0);
            return 0;
        }
        free(item);
    }
    
    printf("  Queue emptied successfully\n");
    
    consumer_producer_destroy(&queue);
    print_test_result("Queue Capacity and Limits", 1);
    return 1;
}

// =============================================================================
// INTERMEDIATE TESTS
// =============================================================================

int test_circular_buffer_wrapping() {
    print_test_header("Circular Buffer Wrapping");
    
    consumer_producer_t queue;
    if (consumer_producer_init(&queue, 3) != 0) {
        print_test_result("Circular Buffer Setup", 0);
        return 0;
    }
    
    printf("  Testing circular buffer behavior...\n");
    
    // Fill, partially empty, then fill again to test wrapping
    // Step 1: Fill queue
    for (int i = 0; i < 3; i++) {
        char* item = create_test_string(i);
        consumer_producer_put(&queue, item);
    }
    
    // Step 2: Remove 2 items
    for (int i = 0; i < 2; i++) {
        char* item = consumer_producer_get(&queue);
        free(item);
    }
    
    // Step 3: Add 2 more items (this should cause wrapping)
    for (int i = 10; i < 12; i++) {
        char* item = create_test_string(i);
        consumer_producer_put(&queue, item);
    }
    
    // Step 4: Verify all remaining items are correct
    char* item1 = consumer_producer_get(&queue);  // Should be test_item_2
    char* item2 = consumer_producer_get(&queue);  // Should be test_item_10
    char* item3 = consumer_producer_get(&queue);  // Should be test_item_11
    
    int success = (strcmp(item1, "test_item_2") == 0 &&
                   strcmp(item2, "test_item_10") == 0 &&
                   strcmp(item3, "test_item_11") == 0);
    
    if (!success) {
        printf("    Wrapping test failed: got '%s', '%s', '%s'\n", item1, item2, item3);
    }
    
    free(item1);
    free(item2);
    free(item3);
    
    consumer_producer_destroy(&queue);
    print_test_result("Circular Buffer Wrapping", success);
    return success;
}

int test_finished_signaling() {
    print_test_header("Finished Signaling Mechanism");
    
    consumer_producer_t queue;
    if (consumer_producer_init(&queue, 5) != 0) {
        print_test_result("Finished Signaling Setup", 0);
        return 0;
    }
    
    printf("  Testing finished signal and wait...\n");
    
    // Test 1: Signal and then wait (should return immediately)
    consumer_producer_signal_finished(&queue);
    int wait_result = consumer_producer_wait_finished(&queue);
    
    if (wait_result != 0) {
        printf("    Wait after signal failed\n");
        consumer_producer_destroy(&queue);
        print_test_result("Finished Signaling", 0);
        return 0;
    }
    
    printf("  Basic finished signaling works\n");
    
    consumer_producer_destroy(&queue);
    print_test_result("Finished Signaling Mechanism", 1);
    return 1;
}

// =============================================================================
// ADVANCED CONCURRENT TESTS
// =============================================================================

void* producer_thread(void* arg) {
    producer_data_t* data = (producer_data_t*)arg;
    
    printf("    Producer %d starting (will produce %d items)\n", 
           data->thread_id, data->items_to_produce);
    
    for (int i = 0; i < data->items_to_produce; i++) {
        char* item = create_test_string(data->start_value + i);
        
        if (consumer_producer_put(data->queue, item) != 0) {
            free(item);
            printf("    Producer %d: put failed at item %d\n", data->thread_id, i);
            break;
        }
        
        data->items_produced++;
        
        // Small random delay to make timing more realistic
        usleep(rand() % 1000);
    }
    
    printf("    Producer %d finished (%d items produced)\n", 
           data->thread_id, data->items_produced);
    return NULL;
}

void* consumer_thread(void* arg) {
    consumer_data_t* data = (consumer_data_t*)arg;
    
    printf("    Consumer %d starting (will consume %d items)\n", 
           data->thread_id, data->items_to_consume);
    
    data->consumed_items = malloc(data->items_to_consume * sizeof(char*));
    
    for (int i = 0; i < data->items_to_consume; i++) {
        char* item = consumer_producer_get(data->queue);
        
        if (item == NULL) {
            printf("    Consumer %d: get returned NULL at item %d\n", data->thread_id, i);
            break;
        }
        
        data->consumed_items[data->items_consumed] = item;  // Store for verification
        data->items_consumed++;
        
        // Small random delay
        usleep(rand() % 1000);
    }
    
    printf("    Consumer %d finished (%d items consumed)\n", 
           data->thread_id, data->items_consumed);
    return NULL;
}

int test_concurrent_producers_consumers() {
    print_test_header("Concurrent Producers and Consumers");
    
    consumer_producer_t queue;
    if (consumer_producer_init(&queue, 10) != 0) {
        print_test_result("Concurrent Test Setup", 0);
        return 0;
    }
    
    const int num_producers = 3;
    const int num_consumers = 2;
    const int items_per_producer = 10;
    const int total_items = num_producers * items_per_producer;
    
    printf("  Setting up %d producers, %d consumers (%d total items)\n",
           num_producers, num_consumers, total_items);
    
    // Create thread data
    producer_data_t producers[num_producers];
    consumer_data_t consumers[num_consumers];
    pthread_t producer_threads[num_producers];
    pthread_t consumer_threads[num_consumers];
    
    // Initialize producer data
    for (int i = 0; i < num_producers; i++) {
        producers[i].queue = &queue;
        producers[i].thread_id = i;
        producers[i].items_to_produce = items_per_producer;
        producers[i].items_produced = 0;
        producers[i].start_value = i * 1000;  // Unique range for each producer
    }
    
    // Initialize consumer data
    int items_per_consumer = total_items / num_consumers;
    int extra_items = total_items % num_consumers;
    
    for (int i = 0; i < num_consumers; i++) {
        consumers[i].queue = &queue;
        consumers[i].thread_id = i;
        consumers[i].items_to_consume = items_per_consumer + (i < extra_items ? 1 : 0);
        consumers[i].items_consumed = 0;
        consumers[i].consumed_items = NULL;
    }
    
    // Start all threads
    printf("  Starting threads...\n");
    
    for (int i = 0; i < num_consumers; i++) {
        pthread_create(&consumer_threads[i], NULL, consumer_thread, &consumers[i]);
    }
    
    for (int i = 0; i < num_producers; i++) {
        pthread_create(&producer_threads[i], NULL, producer_thread, &producers[i]);
    }
    
    // Wait for all producers to finish
    for (int i = 0; i < num_producers; i++) {
        pthread_join(producer_threads[i], NULL);
    }
    
    // Wait for all consumers to finish
    for (int i = 0; i < num_consumers; i++) {
        pthread_join(consumer_threads[i], NULL);
    }
    
    // Verify results
    printf("  Verifying results...\n");
    
    int total_produced = 0;
    int total_consumed = 0;
    
    for (int i = 0; i < num_producers; i++) {
        total_produced += producers[i].items_produced;
    }
    
    for (int i = 0; i < num_consumers; i++) {
        total_consumed += consumers[i].items_consumed;
    }
    
    printf("  Total produced: %d, Total consumed: %d\n", total_produced, total_consumed);
    
    int success = (total_produced == total_items && total_consumed == total_items);
    
    // Clean up consumed items
    for (int i = 0; i < num_consumers; i++) {
        if (consumers[i].consumed_items) {
            for (int j = 0; j < consumers[i].items_consumed; j++) {
                free(consumers[i].consumed_items[j]);
            }
            free(consumers[i].consumed_items);
        }
    }
    
    consumer_producer_destroy(&queue);
    print_test_result("Concurrent Producers and Consumers", success);
    return success;
}

// =============================================================================
// STRESS TESTS
// =============================================================================

void* stress_producer_thread(void* arg) {
    producer_data_t* data = (producer_data_t*)arg;
    time_t start_time = time(NULL);
    
    while (time(NULL) - start_time < STRESS_TEST_DURATION) {
        char* item = create_test_string(data->items_produced);
        
        if (consumer_producer_put(data->queue, item) == 0) {
            data->items_produced++;
        } else {
            free(item);
        }
        
        // Very small delay to prevent overwhelming
        if (data->items_produced % 100 == 0) {
            usleep(1);
        }
    }
    
    return NULL;
}

void* stress_consumer_thread(void* arg) {
    consumer_data_t* data = (consumer_data_t*)arg;
    time_t start_time = time(NULL);
    
    while (time(NULL) - start_time < STRESS_TEST_DURATION) {
        char* item = consumer_producer_get(data->queue);
        
        if (item != NULL) {
            data->items_consumed++;
            free(item);
        }
        
        // Very small delay
        if (data->items_consumed % 100 == 0) {
            usleep(1);
        }
    }
    
    return NULL;
}

int test_stress_high_frequency() {
    print_test_header("Stress Test - High Frequency Operations");
    
    consumer_producer_t queue;
    if (consumer_producer_init(&queue, 50) != 0) {  // Larger capacity for stress
        print_test_result("Stress Test Setup", 0);
        return 0;
    }
    
    const int num_producers = 4;
    const int num_consumers = 4;
    
    printf("  Running stress test for %d seconds with %d producers and %d consumers...\n",
           STRESS_TEST_DURATION, num_producers, num_consumers);
    
    producer_data_t producers[num_producers];
    consumer_data_t consumers[num_consumers];
    pthread_t producer_threads[num_producers];
    pthread_t consumer_threads[num_consumers];
    
    // Initialize data structures
    for (int i = 0; i < num_producers; i++) {
        producers[i].queue = &queue;
        producers[i].thread_id = i;
        producers[i].items_produced = 0;
    }
    
    for (int i = 0; i < num_consumers; i++) {
        consumers[i].queue = &queue;
        consumers[i].thread_id = i;
        consumers[i].items_consumed = 0;
    }
    
    // Start stress test threads
    for (int i = 0; i < num_consumers; i++) {
        pthread_create(&consumer_threads[i], NULL, stress_consumer_thread, &consumers[i]);
    }
    
    for (int i = 0; i < num_producers; i++) {
        pthread_create(&producer_threads[i], NULL, stress_producer_thread, &producers[i]);
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < num_producers; i++) {
        pthread_join(producer_threads[i], NULL);
    }
    
    for (int i = 0; i < num_consumers; i++) {
        pthread_join(consumer_threads[i], NULL);
    }
    
    // Calculate results
    int total_produced = 0;
    int total_consumed = 0;
    
    for (int i = 0; i < num_producers; i++) {
        total_produced += producers[i].items_produced;
        printf("  Producer %d: %d items\n", i, producers[i].items_produced);
    }
    
    for (int i = 0; i < num_consumers; i++) {
        total_consumed += consumers[i].items_consumed;
        printf("  Consumer %d: %d items\n", i, consumers[i].items_consumed);
    }
    
    printf("  Stress test results: %d produced, %d consumed\n", total_produced, total_consumed);
    
    // For stress test, we expect high throughput and roughly balanced production/consumption
    int success = (total_produced > 1000 && total_consumed > 1000 && 
                   abs(total_produced - total_consumed) < total_produced * 0.1);
    
    consumer_producer_destroy(&queue);
    print_test_result("Stress Test - High Frequency Operations", success);
    return success;
}

// =============================================================================
// MAIN TEST RUNNER
// =============================================================================

int main() {
    printf("=================================================================\n");
    printf("         CONSUMER-PRODUCER QUEUE COMPREHENSIVE TEST SUITE        \n");
    printf("=================================================================\n");
    
    srand(time(NULL));  // For random delays in tests
    
    // Run all test categories
    printf("\n🔧 BASIC TESTS\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    test_init_destroy();
    test_single_producer_consumer();
    test_queue_capacity_limits();
    
    printf("\n🔧 EDGE CASE TESTS\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    test_null_pointer_handling();
    test_invalid_parameters();
    
    printf("\n🔧 INTERMEDIATE TESTS\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    test_circular_buffer_wrapping();
    test_finished_signaling();
    
    printf("\n🔧 BLOCKING BEHAVIOR TESTS\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    test_blocking_behavior();
    test_finished_signal_timing();
    
    printf("\n🔧 ADVANCED CONCURRENT TESTS\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    test_concurrent_producers_consumers();
    
    printf("\n🔧 STRESS TESTS\n");
    printf("─────────────────────────────────────────────────────────────────\n");
    test_stress_high_frequency();
    
    // Final summary
    printf("\n=================================================================\n");
    printf("                           TEST SUMMARY                          \n");
    printf("=================================================================\n");
    printf("Total Tests:  %d\n", g_results.total_tests);
    printf("Passed:       %d ✅\n", g_results.passed_tests);
    printf("Failed:       %d ❌\n", g_results.failed_tests);
    printf("Success Rate: %.1f%%\n", 
           (float)g_results.passed_tests / g_results.total_tests * 100);
    
    if (g_results.failed_tests == 0) {
        printf("\n🎉 ALL TESTS PASSED! Your consumer-producer queue is working correctly.\n");
        return 0;
    } else {
        printf("\n⚠️  Some tests failed. Please check your implementation.\n");
        return 1;
    }
}



//gcc tests/consumer_producer_test.c \
    plugins/sync/consumer_producer.c \
    plugins/sync/monitor.c \
    -Iplugins/sync \
    -lpthread \
    -o tests/test_runner

//./tests/test_runner
