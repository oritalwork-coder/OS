// main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>

// Function pointer types for plugin interface
typedef const char* (*plugin_get_name_func_t)(void);
typedef const char* (*plugin_init_func_t)(int);
typedef const char* (*plugin_fini_func_t)(void);
typedef const char* (*plugin_place_work_func_t)(const char*);
typedef void (*plugin_attach_func_t)(const char* (*)(const char*));
typedef const char* (*plugin_wait_finished_func_t)(void);

// Plugin handle structure - exactly as specified in the assignment
typedef struct {
    plugin_init_func_t init;
    plugin_fini_func_t fini;
    plugin_place_work_func_t place_work;
    plugin_attach_func_t attach;
    plugin_wait_finished_func_t wait_finished;
    char* name;
    void* handle;
} plugin_handle_t;

// Global array to store plugin handles
static plugin_handle_t* plugins = NULL;
static int plugin_count = 0;

/**
 * Print usage information to stdout (as required by assignment)
 */
void print_usage(void) {
    printf("Usage: ./analyzer <queue_size> <plugin1> <plugin2> ... <pluginN>\n");
    printf("Arguments:\n");
    printf("  queue_size  Maximum number of items in each plugin's queue\n");
    printf("  plugin1..N  Names of plugins to load (without .so extension)\n");
    printf("Available plugins:\n");
    printf("  logger      - Logs all strings that pass through\n");
    printf("  typewriter  - Simulates typewriter effect with delays\n");
    printf("  uppercaser  - Converts strings to uppercase\n");
    printf("  rotator     - Move every character to the right. Last character moves to the beginning.\n");
    printf("  flipper     - Reverses the order of characters\n");
    printf("  expander    - Expands each character with spaces\n");
    printf("Example:\n");
    printf("  ./analyzer 20 uppercaser rotator logger\n");
    printf("  echo 'hello' | ./analyzer 20 uppercaser rotator logger\n");
    printf("  echo '<END>' | ./analyzer 20 uppercaser rotator logger\n");
}

/**
 * Clean up loaded plugins - handles partial cleanup gracefully
 * @param loaded_count Number of successfully loaded plugins
 * @param initialized_count Number of successfully initialized plugins
 */
void cleanup_plugins(int loaded_count, int initialized_count) {
    if (!plugins) return;
    
    // First, call fini for each initialized plugin
    for (int i = 0; i < initialized_count && i < loaded_count; i++) {
        if (plugins[i].fini) {
            const char* err = plugins[i].fini();
            if (err) {
                fprintf(stderr, "[ERROR] Failed to finalize plugin %s: %s\n", 
                        plugins[i].name ? plugins[i].name : "unknown", err);
            }
        }
    }
    
    // Then free names and close handles for all loaded plugins
    for (int i = 0; i < loaded_count; i++) {
        if (plugins[i].name) {
            free(plugins[i].name);
            plugins[i].name = NULL;
        }
        if (plugins[i].handle) {
            if (dlclose(plugins[i].handle) != 0) {
                fprintf(stderr, "[ERROR] Failed to close plugin handle: %s\n", dlerror());
            }
            plugins[i].handle = NULL;
        }
    }
    
    // Finally, free the plugins array
    free(plugins);
    plugins = NULL;
    plugin_count = 0;
}

/**
 * Load a plugin from a shared object file
 * @param plugin_name Name of the plugin (without .so extension)
 * @param handle Pointer to plugin_handle_t to fill
 * @return 0 on success, -1 on failure
 */
int load_plugin(const char* plugin_name, plugin_handle_t* handle) {
    if (!plugin_name || !handle) {
        fprintf(stderr, "[ERROR] Invalid parameters to load_plugin\n");
        return -1;
    }
    
    // Clear the handle structure
    memset(handle, 0, sizeof(plugin_handle_t));
    
    // Store the plugin name first
    handle->name = strdup(plugin_name);
    if (!handle->name) {
        fprintf(stderr, "[ERROR] Failed to allocate memory for plugin name\n");
        return -1;
    }
    
    // Construct the plugin filename by appending .so
    char filename[256];
    snprintf(filename, sizeof(filename), "./output/%s.so", plugin_name);
    
    // Load the shared object with RTLD_NOW | RTLD_LOCAL as specified
    handle->handle = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
    if (!handle->handle) {
        fprintf(stderr, "[ERROR] Failed to load plugin %s: %s\n", 
                plugin_name, dlerror());
        free(handle->name);
        handle->name = NULL;
        return -1;
    }
    
    // Clear any existing error
    dlerror();
    
    // Resolve all required functions using dlsym
    handle->init = (plugin_init_func_t)dlsym(handle->handle, "plugin_init");
    if (!handle->init) {
        fprintf(stderr, "[ERROR] Failed to find plugin_init in %s: %s\n", 
                plugin_name, dlerror());
        dlclose(handle->handle);
        free(handle->name);
        handle->name = NULL;
        return -1;
    }
    
    handle->fini = (plugin_fini_func_t)dlsym(handle->handle, "plugin_fini");
    if (!handle->fini) {
        fprintf(stderr, "[ERROR] Failed to find plugin_fini in %s: %s\n", 
                plugin_name, dlerror());
        dlclose(handle->handle);
        free(handle->name);
        handle->name = NULL;
        return -1;
    }
    
    handle->place_work = (plugin_place_work_func_t)dlsym(handle->handle, "plugin_place_work");
    if (!handle->place_work) {
        fprintf(stderr, "[ERROR] Failed to find plugin_place_work in %s: %s\n", 
                plugin_name, dlerror());
        dlclose(handle->handle);
        free(handle->name);
        handle->name = NULL;
        return -1;
    }
    
    handle->attach = (plugin_attach_func_t)dlsym(handle->handle, "plugin_attach");
    if (!handle->attach) {
        fprintf(stderr, "[ERROR] Failed to find plugin_attach in %s: %s\n", 
                plugin_name, dlerror());
        dlclose(handle->handle);
        free(handle->name);
        handle->name = NULL;
        return -1;
    }
    
    handle->wait_finished = (plugin_wait_finished_func_t)dlsym(handle->handle, "plugin_wait_finished");
    if (!handle->wait_finished) {
        fprintf(stderr, "[ERROR] Failed to find plugin_wait_finished in %s: %s\n", 
                plugin_name, dlerror());
        dlclose(handle->handle);
        free(handle->name);
        handle->name = NULL;
        return -1;
    }
    
    return 0;
}

int main(int argc, char* argv[]) {
    // Step 1: Parse Command-Line Arguments
    if (argc < 3) {
        fprintf(stderr, "[ERROR] Insufficient arguments\n");
        print_usage();
        return 1;  // Exit code 1 for argument errors
    }
    
    // Parse queue size - must be a positive integer
    char* endptr;
    long queue_size_long = strtol(argv[1], &endptr, 10);
    
    // Validate queue size
    if (*endptr != '\0' || queue_size_long <= 0 || queue_size_long > INT_MAX) {
        fprintf(stderr, "[ERROR] Invalid queue size: %s (must be a positive integer)\n", argv[1]);
        print_usage();
        return 1;  // Exit code 1 for invalid arguments
    }
    
    int queue_size = (int)queue_size_long;
    
    // Calculate number of plugins
    plugin_count = argc - 2;
    if (plugin_count <= 0) {
        fprintf(stderr, "[ERROR] No plugins specified\n");
        print_usage();
        return 1;  // Exit code 1 for missing arguments
    }
    
    // Allocate memory for plugin handles
    plugins = calloc(plugin_count, sizeof(plugin_handle_t));
    if (!plugins) {
        fprintf(stderr, "[ERROR] Failed to allocate memory for plugins\n");
        return 1;  // Exit code 1 for memory allocation failure
    }
    
    // Step 2: Load Plugin Shared Objects
    int loaded_count = 0;
    for (int i = 0; i < plugin_count; i++) {
        if (load_plugin(argv[i + 2], &plugins[i]) != 0) {
            // On any failure: print error to stderr, print usage to stdout, exit with code 1
            print_usage();
            cleanup_plugins(loaded_count, 0);
            return 1;  // Exit code 1 for plugin loading failure
        }
        loaded_count++;
    }
    
    // Step 3: Initialize Plugins
    int initialized_count = 0;
    for (int i = 0; i < plugin_count; i++) {
        const char* err = plugins[i].init(queue_size);
        if (err) {
            // If plugin fails to initialize: stop, clean up, print error, exit with code 2
            fprintf(stderr, "[ERROR] Failed to initialize plugin %s: %s\n", 
                    plugins[i].name, err);
            cleanup_plugins(loaded_count, initialized_count);
            return 2;  // Exit code 2 for plugin initialization failure
        }
        initialized_count++;
    }
    
    // Step 4: Attach Plugins Together
    for (int i = 0; i < plugin_count - 1; i++) {
        // For plugin i, attach to plugin[i+1].place_work
        plugins[i].attach(plugins[i + 1].place_work);
    }
    // Do not attach the last plugin to anything
    if (plugin_count > 0) {
        plugins[plugin_count - 1].attach(NULL);
    }
    
    // Step 5: Read Input from STDIN
    char line[1024];
    int end_received = 0;
    
    while (fgets(line, sizeof(line), stdin) != NULL) {
        // Remove trailing newline if present
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        // Send the string to the first plugin using its place_work function
        const char* err = plugins[0].place_work(line);
        if (err) {
            fprintf(stderr, "[ERROR] Failed to place work: %s\n", err);
            // Continue processing - don't exit on place_work error
        }
        
        // If the string is exactly <END>, send it and break
        if (strcmp(line, "<END>") == 0) {
            end_received = 1;
            break;
        }
    }
    
    // If we didn't receive <END> from input but stdin closed, send it anyway for graceful shutdown
    if (!end_received) {
        const char* err = plugins[0].place_work("<END>");
        if (err) {
            fprintf(stderr, "[ERROR] Failed to send <END>: %s\n", err);
        }
    }
    
    // Step 6: Wait for Plugins to Finish
    // Call each plugin's wait_finished() in ascending order (from first to last)
    for (int i = 0; i < plugin_count; i++) {
        const char* err = plugins[i].wait_finished();
        if (err) {
            fprintf(stderr, "[ERROR] Failed waiting for plugin %s to finish: %s\n", 
                    plugins[i].name, err);
            // Continue waiting for other plugins - don't exit early
        }
    }
    
    // Step 7: Cleanup
    // This function will:
    // - Call each plugin's fini() to cleanup memory
    // - Free memory allocated by main thread
    // - Unload each plugin using dlclose()
    cleanup_plugins(loaded_count, initialized_count);
    
    // Step 8: Finalize
    printf("Pipeline shutdown complete\n");
    
    return 0;  // Exit code 0 for success
}