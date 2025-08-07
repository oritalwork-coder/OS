// flipper.c
#include "plugin_common.h"
#include <stdlib.h>
#include <string.h>

/**
 * Flipper plugin transformation function
 * Reverses the order of characters in the string
 * @param input The input string to transform
 * @return A new reversed string (caller must free)
 */
const char* flipper_transform(const char* input) {
    if (!input) return NULL;
    
    size_t len = strlen(input);
    
    // Allocate memory for the output string
    char* output = malloc(len + 1);
    if (!output) {
        return NULL;
    }
    
    // Reverse the string
    for (size_t i = 0; i < len; i++) {
        output[i] = input[len - 1 - i];
    }
    output[len] = '\0';
    
    return output;
}

/**
 * Initialize the flipper plugin
 * @param queue_size Maximum number of items that can be queued
 * @return NULL on success, error message on failure
 */
const char* plugin_init(int queue_size) {
    return common_plugin_init(flipper_transform, "flipper", queue_size);
}