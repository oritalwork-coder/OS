// rotator.c
#include "plugin_common.h"
#include <stdlib.h>
#include <string.h>

/**
 * Rotator plugin transformation function
 * Moves every character in the string one position to the right
 * The last character wraps around to the front
 * @param input The input string to transform
 * @return A new rotated string (caller must free)
 */
const char* rotator_transform(const char* input) {
    if (!input) return NULL;
    
    size_t len = strlen(input);
    
    // Handle empty string
    if (len == 0) {
        return strdup("");
    }
    
    // Allocate memory for the output string
    char* output = malloc(len + 1);
    if (!output) {
        return NULL;
    }
    
    // Handle single character string
    if (len == 1) {
        strcpy(output, input);
        return output;
    }
    
    // Rotate: last character moves to front, others shift right
    output[0] = input[len - 1];  // Last character to front
    for (size_t i = 1; i < len; i++) {
        output[i] = input[i - 1];  // Shift others right
    }
    output[len] = '\0';
    
    return output;
}

/**
 * Initialize the rotator plugin
 * @param queue_size Maximum number of items that can be queued
 * @return NULL on success, error message on failure
 */
const char* plugin_init(int queue_size) {
    return common_plugin_init(rotator_transform, "rotator", queue_size);
}