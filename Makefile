# Makefile for Disting NT Grids Plugin

# Compiler and flags (Adjust path if necessary)
CXX = arm-none-eabi-g++
CFLAGS = -std=c++11 -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb \
         -Os -Wall -fno-rtti -fno-exceptions

# Include paths
INCLUDES = -I. -I./distingNT_API/include

# Source files
SOURCES = nt_grids.cc nt_grids_pattern_generator.cc nt_grids_resources.cc nt_grids_utils.cc nt_grids_takeover_pot.cc \
          disting_nt_platform_adapter.cc nt_grids_drum_mode.cc nt_grids_euclidean_mode.cc plugin_allocator.cc

# Object files (derived from sources)
OBJECTS = $(patsubst %.cc, build/%.o, $(SOURCES))

# Output plugin file
OUTPUT_DIR = plugins
OUTPUT_PLUGIN = $(OUTPUT_DIR)/nt_grids.o

# Build directory
BUILD_DIR = build

# Default target
all: $(OUTPUT_PLUGIN)

# Rule to build the output plugin object file from individual objects
$(OUTPUT_PLUGIN): $(OBJECTS)
	mkdir -p $(OUTPUT_DIR)
	$(CXX) $(CFLAGS) -Wl,--relocatable -nostdlib -o $@ $^

# Rule to compile .cc files into .o files in the build directory
$(BUILD_DIR)/%.o: %.cc | $(BUILD_DIR)
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean target
clean:
	rm -rf $(BUILD_DIR) $(OUTPUT_DIR)
	@echo "Cleaned build and output directories"

# Target to check for undefined symbols in the plugin
check: all
	@echo "Checking for undefined symbols in $(OUTPUT_PLUGIN)..."
	@arm-none-eabi-nm $(OUTPUT_PLUGIN) | grep ' U ' || echo "No undefined symbols found (or grep failed to find any)."
	@echo "Note: If symbols are listed above, they are undefined in the plugin and expected to be provided by the host."

.PHONY: all clean check 
