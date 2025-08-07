// plugin_common.c
#include "sync/consumer_producer.h"  
#include "../sync/consumer_producer.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "plugin_common.h"

// Static context for this plugin instance
static plugin_context_t context;
static consumer_producer_t work_queue;
static pthread_mutex_t context_mutex = PTHREAD_MUTEX_INITIALIZER;


// Thread function that processes items from the queue
void* plugin_consumer_thread(void* arg) {
    plugin_context_t* ctx = (plugin_context_t*)arg;
    
    if (!ctx || !ctx->queue || !ctx->process_function) {
        return NULL;
    }
    log_info(ctx, "Consumer thread started");
    char* item;
    while ((item = consumer_producer_get(ctx->queue)) != NULL) {
        // Special handling for <END>
        if (strcmp(item, "<END>") == 0) {
            log_info(ctx, "Received <END>, shutting down");
            // Forward <END> to next plugin if attached
            if (ctx->next_place_work) {
                const char* err = ctx->next_place_work("<END>");
                if (err) {
                    log_error(ctx, err);
                }
            }
            
            free(item);
            break;  // Exit the loop
        }
        
        // Transform the string
        const char* transformed = ctx->process_function(item);
        free(item);  // Free the input string
        
        if (!transformed) {
            log_error(ctx, "process_function returned NULL");
            continue;
        }
        
        // Forward to next plugin or print if last in chain
        if (ctx->next_place_work) {
            const char* err = ctx->next_place_work(transformed);
            if (err) {
                log_error(ctx, err);
            }
        } else {
            // Last plugin in chain - output result
            printf("%s\n", transformed);
        }
        // Free the transformed string if it was allocated
        free((void*)transformed);
    }
    
    log_info(ctx, "Consumer thread exiting");
    pthread_mutex_lock(&context_mutex);
    ctx->finished = 1;
    pthread_mutex_unlock(&context_mutex);
    return NULL;
}

// Log error message
void log_error(plugin_context_t* ctx, const char* message) {
    if (ctx && ctx->name && message) {
        fprintf(stderr, "[ERROR][%s] - %s\n", ctx->name, message);
    }
}

// Log info message
void log_info(plugin_context_t* ctx, const char* message) {
    if (ctx && ctx->name && message) {
        fprintf(stdout, "[INFO][%s] - %s\n", ctx->name, message);
    }
}

// Get the plugin's name
const char* plugin_get_name(void) {
    const char* name = NULL;
    pthread_mutex_lock(&context_mutex);
    name = context.name;
    pthread_mutex_unlock(&context_mutex);
    return name;
}

// Initialize the common plugin infrastructure
const char* common_plugin_init(const char* (*process_function)(const char*),   
                              const char* name, int queue_size) {
    if (!process_function || !name || queue_size <= 0) {
        return "Invalid arguments to common_plugin_init";
    }
    pthread_mutex_lock(&context_mutex);
    // Check if already initialized
    if (context.initialized) {
        pthread_mutex_unlock(&context_mutex);
        return "Plugin already initialized";
    }
    
    // Initialize context
    memset(&context, 0, sizeof(context));
    context.name = name;
    context.process_function = process_function;
    context.next_place_work = NULL;
    context.finished = 0;
    
    // Initialize the queue
    const char* err = consumer_producer_init(&work_queue, queue_size);
    if (err) {
        pthread_mutex_unlock(&context_mutex);
        return err;
    }
    
    context.queue = &work_queue;
    // Create consumer thread
    if (pthread_create(&context.consumer_thread, NULL,
                       plugin_consumer_thread, &context) != 0) {
        consumer_producer_destroy(&work_queue);
        pthread_mutex_unlock(&context_mutex);
        return "Failed to create consumer thread";
    }
    context.initialized = 1;
    pthread_mutex_unlock(&context_mutex);
    
    log_info(&context, "Plugin initialized successfully");
    return NULL;
}
// Finalize the plugin
const char* plugin_fini(void) {
    pthread_mutex_lock(&context_mutex);
    if (!context.initialized) {
        pthread_mutex_unlock(&context_mutex);
        return "Plugin not initialized";
    }
    
    log_info(&context, "Finalizing plugin");
    // Signal the queue to finish
    consumer_producer_signal_finished(context.queue);
    pthread_mutex_unlock(&context_mutex);
    // Wait for consumer thread to finish
    if (pthread_join(context.consumer_thread, NULL) != 0) {
        return "Failed to join consumer thread";
    }
    pthread_mutex_lock(&context_mutex);
    // Destroy the queue
    consumer_producer_destroy(context.queue);
    // Reset context
    context.initialized = 0;
    context.finished = 0;
    context.next_place_work = NULL;
    context.process_function = NULL;
    context.name = NULL;
    context.queue = NULL;
    pthread_mutex_unlock(&context_mutex);
    return NULL;
}
// Place work into the plugin's queue
const char* plugin_place_work(const char* str) {
    if (!str) {
        return "NULL string provided";
    }
    pthread_mutex_lock(&context_mutex);
    if (!context.initialized) {
        pthread_mutex_unlock(&context_mutex);
        return "Plugin not initialized";
    }
    // Check if already finished
    if (context.finished) {
        pthread_mutex_unlock(&context_mutex);
        return "Plugin already finished processing";
    }
    
    pthread_mutex_unlock(&context_mutex);
    // Put the string in the queue (this will block if queue is full)
    const char* err = consumer_producer_put(context.queue, str);
    if (err) {
        return err;
    }
    
    // Special handling for <END>
    if (strcmp(str, "<END>") == 0) {
        // Signal queue finished for graceful shutdown
        consumer_producer_signal_finished(context.queue);
    }
    return NULL;
}

// Attach this plugin to the next plugin in the chain
void plugin_attach(const char* (*next_place_work)(const char*)) {
    pthread_mutex_lock(&context_mutex);  
    context.next_place_work = next_place_work;
    if (next_place_work) {
        log_info(&context, "Attached to next plugin");
    } else {
        log_info(&context, "Detached from next plugin (now last in chain)");
    }
    
    pthread_mutex_unlock(&context_mutex);
}

// Wait until the plugin has finished processing
const char* plugin_wait_finished(void) {
    pthread_mutex_lock(&context_mutex);
    if (!context.initialized) {
        pthread_mutex_unlock(&context_mutex);
        return "Plugin not initialized";
    }
    pthread_mutex_unlock(&context_mutex);
    
    log_info(&context, "Waiting for plugin to finish");
    
    // Wait for the queue to finish processing
    if (consumer_producer_wait_finished(context.queue) != 0) {
        return "Failed to wait for queue to finish";
    }
    
    // Wait for consumer thread to set finished flag
    int finished = 0;
    while (!finished) {
        pthread_mutex_lock(&context_mutex);
        finished = context.finished;
        pthread_mutex_unlock(&context_mutex);
        
        if (!finished) {
            usleep(1000);  // 1ms sleep to avoid busy waiting
        }
    }
    
    log_info(&context, "Plugin finished processing");
    return NULL;
}