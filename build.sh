#!/bin/bash

# Exit immediately if any command fails
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[BUILD]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Clean start
print_status "Starting build process..."

# Create output directory if it doesn't exist
if [ ! -d "output" ]; then
    print_status "Creating output directory..."
    mkdir output
else
    print_status "Cleaning output directory..."
    rm -f output/*
fi

# Build the main application
print_status "Building main application..."
gcc -o output/analyzer \
    main.c \
    -ldl -lpthread \
    -Wall -Wextra -g || {
    print_error "Failed to build main application"
    exit 1
}
print_status "Main application built successfully"

# Build shared libraries for synchronization and common infrastructure
print_status "Building synchronization libraries..."

# List of plugins to build
PLUGINS="logger uppercaser rotator flipper expander typewriter"

# Build each plugin as a shared object
for plugin in $PLUGINS; do
    plugin_file="plugins/${plugin}.c"
    
    # Check if plugin source file exists
    if [ ! -f "$plugin_file" ]; then
        print_warning "Plugin source file not found: $plugin_file (skipping)"
        continue
    fi
    
    print_status "Building plugin: ${plugin}"
    
    gcc -fPIC -shared -o output/${plugin}.so \
        plugins/${plugin}.c \
        plugins/plugin_common.c \
        plugins/sync/monitor.c \
        plugins/sync/consumer_producer.c \
        -Iplugins \
        -Iplugins/sync \
        -lpthread \
        -Wall -Wextra -g || {
        print_error "Failed to build plugin: ${plugin}"
        exit 1
    }
    
    print_status "Plugin ${plugin} built successfully"
done

# Build test executables (optional - for unit testing)
print_status "Building test executables..."

# Build monitor test if it exists
if [ -f "plugins/sync/monitor_test.c" ]; then
    print_status "Building monitor test..."
    gcc -o output/monitor_test \
        plugins/sync/monitor_test.c \
        plugins/sync/monitor.c \
        -Iplugins/sync \
        -lpthread \
        -Wall -Wextra -g || {
        print_warning "Failed to build monitor test (non-critical)"
    }
fi

# Build consumer-producer test if it exists
if [ -f "plugins/sync/consumer_producer_test.c" ]; then
    print_status "Building consumer-producer test..."
    gcc -o output/consumer_producer_test \
        plugins/sync/consumer_producer_test.c \
        plugins/sync/consumer_producer.c \
        plugins/sync/monitor.c \
        -Iplugins/sync \
        -lpthread \
        -Wall -Wextra -g || {
        print_warning "Failed to build consumer-producer test (non-critical)"
    }
fi

print_status "Build process completed successfully!"
print_status "Executable location: ./output/analyzer"
print_status "Plugin locations: ./output/*.so"

# Display usage hint
echo ""
echo "To run the analyzer:"
echo "  ./output/analyzer <queue_size> <plugin1> <plugin2> ..."
echo ""
echo "Example:"
echo "  echo 'hello' | ./output/analyzer 10 uppercaser logger"
echo "  echo -e 'hello\\nworld\\n<END>' | ./output/analyzer 10 uppercaser rotator logger"
echo ""
echo "Available plugins:"
for plugin in $PLUGINS; do
    if [ -f "output/${plugin}.so" ]; then
        echo "  - ${plugin}"
    fi
done