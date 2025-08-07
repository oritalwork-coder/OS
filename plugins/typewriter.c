// typewriter.c
#include "plugin_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/**
 * Typewriter plugin transformation function
 * Adds "[typewriter] " prefix and prints each character with 100ms delay
 * @param input The input string to transform
 * @return A new string with the typewriter prefix (caller must free)
 */
const char* typewriter_transform(const char* input) {
    if (!input) return NULL;
    
    // Calculate required size: "[typewriter] " + input + null terminator
    size_t prefix_len = strlen("[typewriter] ");
    size_t input_len = strlen(input);
    size_t total_len = prefix_len + input_len + 1;
    
    // Allocate memory for the new string
    char* output = malloc(total_len);
    if (!output) {
        return NULL;
    }
    
    // Build the output string
    snprintf(output, total_len, "[typewriter] %s", input);
    
    // Print each character with delay (typewriter effect)
    printf("[typewriter] ");
    fflush(stdout);
    
    for (size_t i = 0; i < input_len; i++) {
        printf("%c", input[i]);
        fflush(stdout);
        usleep(100000);  // 100ms delay
    }
    printf("\n");
    fflush(stdout);
    
    return output;
}

/**
 * Initialize the typewriter plugin
 * @param queue_size Maximum number of items that can be queued
 * @return NULL on success, error message on failure
 */
const char* plugin_init(int queue_size) {
    return common_plugin_init(typewriter_transform, "typewriter", queue_size);
}