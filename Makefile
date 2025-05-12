# Makefile for nt_grids Disting NT plugin

# Path to the Disting NT API
NT_API_PATH := ./distingNT_API

# Path to the Mutable Instruments Eurorack submodule
EURORACK_PATH := eurorack

# Path to Grids specific code (original, not directly used by our compiled sources)
# GRIDS_PATH := $(EURORACK_PATH)/grids 

# Include paths for nt_grids plugin
INCLUDE_PATHS = \
    -I. \
    -I$(NT_API_PATH)/include

# Compiler
CC := arm-none-eabi-c++

# Common CFLAGS for compiling individual .cc to .o files
COMPILE_CFLAGS = \
    -std=c++11 \
    -mcpu=cortex-m7 \
    -mfpu=fpv5-d16 \
    -mfloat-abi=hard \
    -mthumb \
    -fno-rtti \
    -fno-exceptions \
    -fno-unwind-tables \
    -fno-asynchronous-unwind-tables \
    -Os \
    -fPIC \
    -fno-pie \
    -Wall \
    $(INCLUDE_PATHS)

# Linker flags for the final plugin object
LINK_FLAGS = \
    -std=c++11 \
    -mcpu=cortex-m7 \
    -mfpu=fpv5-d16 \
    -mfloat-abi=hard \
    -mthumb \
    -fno-rtti \
    -fno-exceptions \
    -fno-unwind-tables \
    -fno-asynchronous-unwind-tables \
    -Os \
    -fPIC \
    -Wall \
    $(INCLUDE_PATHS) \
    -Wl,-r \
    -nostartfiles

# Source files
SRCS = \
    nt_grids.cc \
    nt_grids_pattern_generator.cc \
    nt_grids_resources.cc \
    nt_grids_utils.cc

# Object files: place them in a build directory
BUILD_DIR := build
OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(filter %.cpp,$(SRCS)))
OBJS += $(patsubst %.cc,$(BUILD_DIR)/%.o,$(filter %.cc,$(SRCS)))


# Output plugin file (placed in a plugins directory)
OUTPUT_DIR := plugins
TARGET := $(OUTPUT_DIR)/nt_grids.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(OUTPUT_DIR)
	$(CC) $(LINK_FLAGS) -o $@ $(OBJS)
	@echo "Built $@ successfully"
	arm-none-eabi-strip --strip-unneeded $@
	@echo "Stripped $@ successfully"
	@echo "--- Section Headers for $(TARGET) (Stripped) ---"
	arm-none-eabi-objdump -h $@
	@echo "--- Symbol Table for $(TARGET) (Stripped) ---"
	arm-none-eabi-nm -n $@
	@echo "--- Relocations for .text (Stripped) ---"
	arm-none-eabi-objdump -r -j .text $@
	@echo "--- Relocations for .rodata (Stripped) ---"
	arm-none-eabi-objdump -r -j .rodata $@
	@echo "--- Relocations for .data (Stripped) ---"
	arm-none-eabi-objdump -r -j .data $@

# Rule to compile .cpp files to intermediate objects
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CC) $(COMPILE_CFLAGS) -c -o $@ $<

# Rule to compile .cc files to intermediate objects
$(BUILD_DIR)/%.o: %.cc
	@mkdir -p $(@D)
	$(CC) $(COMPILE_CFLAGS) -c -o $@ $<

# Rule to compile .cc files from Grids (original - not used for linking nt_grids.o)
# $(BUILD_DIR)/eurorack/grids/%.o: $(EURORACK_PATH)/grids/%.cc
# 	@mkdir -p $(@D)
# 	$(CC) $(CFLAGS) -c -o $@ $<

# Add a specific rule for resources.cc if it needs special handling or is part of pattern_generator.o
# For now, pattern_generator.cc and resources.cc will be compiled separately.

clean:
	rm -rf $(BUILD_DIR) $(OUTPUT_DIR)
	@echo "Cleaned build and output directories"

# Note: This Makefile assumes pattern_generator.cc and resources.cc can be compiled directly.
# It also assumes they don't have conflicting symbols or main functions.
# The Grids code is for an AVR microcontroller, so there might be incompatibilities
# or missing dependencies when compiling for ARM Cortex-M7 (Disting NT).
# This is a starting point and will likely need refinement. 