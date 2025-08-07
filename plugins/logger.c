// logger.c
#include "plugin_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * Logger plugin transformation function
 * Adds "[logger] " prefix to the input string
 * @param input The input string to transform
 * @return A new string with the logger prefix (caller must free)
 */
const char* logger_transform(const char* input) {
    if (!input) return NULL;
    
    // Calculate required size: "[logger] " + input + null terminator
    size_t prefix_len = strlen("[logger] ");
    size_t input_len = strlen(input);
    size_t total_len = prefix_len + input_len + 1;
    
    // Allocate memory for the new string
    char* output = malloc(total_len);
    if (!output) {
        return NULL;
    }
    
    // Build the output string
    snprintf(output, total_len, "[logger] %s", input);
    
    return output;
}

/**
 * Initialize the logger plugin
 * @param queue_size Maximum number of items that can be queued
 * @return NULL on success, error message on failure
 */
const char* plugin_init(int queue_size) {
    return common_plugin_init(logger_transform, "logger", queue_size);
}