// uppercaser.c
#include "plugin_common.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/**
 * Uppercaser plugin transformation function
 * Converts all alphabetic characters in the string to uppercase
 * @param input The input string to transform
 * @return A new uppercase string (caller must free)
 */
const char* uppercaser_transform(const char* input) {
    if (!input) return NULL;
    
    // Allocate memory for the output string
    size_t len = strlen(input);
    char* output = malloc(len + 1);
    if (!output) {
        return NULL;
    }
    
    // Convert each character to uppercase
    for (size_t i = 0; i < len; i++) {
        output[i] = toupper(input[i]);
    }
    output[len] = '\0';
    
    return output;
}

/**
 * Initialize the uppercaser plugin
 * @param queue_size Maximum number of items that can be queued
 * @return NULL on success, error message on failure
 */
const char* plugin_init(int queue_size) {
    return common_plugin_init(uppercaser_transform, "uppercaser", queue_size);
}