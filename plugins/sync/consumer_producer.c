// consumer_producer.c
#include "consumer_producer.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// initialize a consumer-producer queue
// Returns NULL on success, error message on failure
const char* consumer_producer_init(consumer_producer_t* queue, int capacity) {
    if (!queue || capacity <= 0) return "Invalid parameters";

    queue->items = malloc(sizeof(char*) * capacity);
    if (!queue->items) {
        return "Memory allocation failed";
    }
    queue->capacity = capacity;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    if (pthread_mutex_init(&queue->lock_queue, NULL) != 0) {
        free(queue->items);
        return "Mutex initialization failed";
    }

    if (monitor_init(&queue->not_full_monitor) != 0 ||
        monitor_init(&queue->not_empty_monitor) != 0 ||
        monitor_init(&queue->finished_monitor) != 0) {
        free(queue->items);
        return "Monitor initialization failed";
    }
    monitor_signal(&queue->not_full_monitor); // Initially not full
    return NULL; // Success
}
//destroy a consumer-producer queue and free its resources
void consumer_producer_destroy(consumer_producer_t* queue) {
    if (!queue) return;

    if (queue->items) {
        for (int i = 0; i < queue->count; i++) {
            int idx = (queue->head + i) % queue->capacity;
            free(queue->items[idx]);
        }
        free(queue->items);
        queue->items = NULL;
    }

    monitor_destroy(&queue->not_full_monitor);
    monitor_destroy(&queue->not_empty_monitor);
    monitor_destroy(&queue->finished_monitor);

    pthread_mutex_destroy(&queue->lock_queue);

    // Reset fields to prevent double free and invalid access
    queue->capacity = 0;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
}
// add an item to the queue (producer), blocks if queue is full
// Returns NULL on success, error message on failure
const char* consumer_producer_put(consumer_producer_t* queue, const char* item) {
    if (!queue || !item) return "Invalid parameters";
    // Wait until there is space in the queue
    if (monitor_wait(&queue->not_full_monitor) != 0) {
        return "Monitor wait failed";
    }
    // Critical section

    pthread_mutex_lock(&queue->lock_queue);
    // Double check if queue is full
    if (queue->count >= queue->capacity) {
        pthread_mutex_unlock(&queue->lock_queue);
        return "Queue is full";
    }
    // Add item to queue (and make a copy of it to a string)
    queue->items[queue->tail] = strdup(item);
    if (!queue->items[queue->tail]) {
        pthread_mutex_unlock(&queue->lock_queue);
        return "Memory allocation failed";
    }

    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;

    // Check if queue is now full
    if (queue->count < queue->capacity) {
        monitor_signal(&queue->not_full_monitor); // Still has space
    } else {
        monitor_reset(&queue->not_full_monitor);  // Now full
    }

    // Queue is no longer empty, signal consumers
    monitor_signal(&queue->not_empty_monitor);
    pthread_mutex_unlock(&queue->lock_queue);
    return NULL; // Success
}

// remove an item from the queue (consumer) and returns it, 
// blocks if queue is empty and returns NULL 
char* consumer_producer_get(consumer_producer_t* queue) {
    if (!queue) return NULL;

    while (1) {
        // Check if we're finished before waiting
        pthread_mutex_lock(&queue->finished_monitor.mutex);
        int is_finished = queue->finished_monitor.signaled;
        pthread_mutex_unlock(&queue->finished_monitor.mutex);
        
        pthread_mutex_lock(&queue->lock_queue);
        int is_empty = (queue->count == 0);
        pthread_mutex_unlock(&queue->lock_queue);
        
        // If finished and empty, return NULL immediately
        if (is_finished && is_empty) {
            return NULL;
        }
        
        // Wait until there is an item in the queue or finished is signaled
        if (monitor_wait(&queue->not_empty_monitor) != 0) {
            return NULL;
        }
        
        // Critical section - access queue
        pthread_mutex_lock(&queue->lock_queue);
        
        // Check if we have items
        if (queue->count > 0) {
            // Remove item from queue
            char* item = queue->items[queue->head];
            queue->head = (queue->head + 1) % queue->capacity;
            queue->count--;
            
            // Check if queue is now empty
            if (queue->count > 0) {
                monitor_signal(&queue->not_empty_monitor); // Still has items
            } else {
                monitor_reset(&queue->not_empty_monitor);  // Now empty
            }
            
            // Queue is no longer full, signal producers
            monitor_signal(&queue->not_full_monitor);

            pthread_mutex_unlock(&queue->lock_queue);
            return item; // Caller takes ownership of the string
        }
        
        // Queue is empty, check if finished
        pthread_mutex_lock(&queue->finished_monitor.mutex);
        is_finished = queue->finished_monitor.signaled;
        pthread_mutex_unlock(&queue->finished_monitor.mutex);
        
        pthread_mutex_unlock(&queue->lock_queue);
        
        if (is_finished) {
            return NULL;  // Queue is empty and finished
        }
        
        // Not finished, loop back to wait again
        // This handles spurious wakeups and race conditions
    }
}

// Signal that processing is finished
void consumer_producer_signal_finished(consumer_producer_t* queue) {
    if (!queue) return;
    monitor_signal(&queue->finished_monitor);
    
    // Wake up any consumers that might be waiting, but don't leave the monitor signaled
    pthread_mutex_lock(&queue->not_empty_monitor.mutex);
    pthread_cond_broadcast(&queue->not_empty_monitor.condition);
    pthread_mutex_unlock(&queue->not_empty_monitor.mutex);
}

// Wait for processing to be finished
int consumer_producer_wait_finished(consumer_producer_t* queue) {
    if (!queue) return -1;
    return monitor_wait(&queue->finished_monitor);
}