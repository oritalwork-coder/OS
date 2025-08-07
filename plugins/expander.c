// expander.c
#include "plugin_common.h"
#include <stdlib.h>
#include <string.h>

/**
 * Expander plugin transformation function
 * Inserts a single white space between each character in the string
 * @param input The input string to transform
 * @return A new expanded string (caller must free)
 */
const char* expander_transform(const char* input) {
    if (!input) return NULL;
    
    size_t len = strlen(input);
    
    // Handle empty string
    if (len == 0) {
        return strdup("");
    }
    
    // Calculate output size: original chars + (len-1) spaces + null terminator
    size_t output_len = len + (len > 0 ? len - 1 : 0) + 1;
    
    // Allocate memory for the output string
    char* output = malloc(output_len);
    if (!output) {
        return NULL;
    }
    
    // Build the expanded string
    size_t output_pos = 0;
    for (size_t i = 0; i < len; i++) {
        output[output_pos++] = input[i];
        
        // Add space after each character except the last one
        if (i < len - 1) {
            output[output_pos++] = ' ';
        }
    }
    output[output_pos] = '\0';
    
    return output;
}

/**
 * Initialize the expander plugin
 * @param queue_size Maximum number of items that can be queued
 * @return NULL on success, error message on failure
 */
const char* plugin_init(int queue_size) {
    return common_plugin_init(expander_transform, "expander", queue_size);
}