TOOL_ROOT ?= /private/tmp/aimer-m7-tools
PREFIX := $(TOOL_ROOT)/gcc/bin/arm-none-eabi-
CC := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump
SIZE := $(PREFIX)size

PARAM ?= 128f
IMPL ?= reference

ifeq ($(IMPL),reference)
AIMER_DIR := ../../Reference_Implementation
else ifeq ($(IMPL),optimized)
AIMER_DIR := ../../gf_inv_opt
else
$(error IMPL must be reference or optimized)
endif

ifneq (,$(findstring 128,$(PARAM)))
FIELD_SOURCE := field128.c
else ifneq (,$(findstring 192,$(PARAM)))
FIELD_SOURCE := field192.c
else ifneq (,$(findstring 256,$(PARAM)))
FIELD_SOURCE := field256.c
else
$(error unsupported PARAM=$(PARAM))
endif

BUILD_DIR := build/$(IMPL)-$(PARAM)
ELF := $(BUILD_DIR)/preflight.elf
MAP := $(BUILD_DIR)/preflight.map
BIN := $(BUILD_DIR)/preflight.bin
DISASM := $(BUILD_DIR)/preflight.disasm.txt

CPU_FLAGS := -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
COMMON_FLAGS := -O3 -std=c11 $(CPU_FLAGS) -fno-lto -ffunction-sections \
	-fdata-sections -fstack-usage -Wall -Wextra -Wpedantic
CFLAGS := $(COMMON_FLAGS) -I$(AIMER_DIR) -DPARAMS=$(PARAM) \
	-DIMPL_NAME=\"$(IMPL)\" -MMD -MP
LDFLAGS := $(CPU_FLAGS) -fno-lto -nostartfiles --specs=nano.specs \
	-Tstm32f767zi.ld -Wl,--gc-sections,-Map,$(MAP) \
	-Wl,--print-memory-usage

LOCAL_NAMES := startup platform allocator preflight
AIMER_NAMES := aim3 field_common hash sign tree common/aes common/fips202 common/rng
LOCAL_OBJECTS := $(addprefix $(BUILD_DIR)/local/,$(addsuffix .o,$(LOCAL_NAMES)))
AIMER_OBJECTS := $(addprefix $(BUILD_DIR)/aimer/,$(addsuffix .o,$(AIMER_NAMES))) \
	$(BUILD_DIR)/aimer/$(FIELD_SOURCE:.c=.o)
OBJECTS := $(LOCAL_OBJECTS) $(AIMER_OBJECTS)
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean matrix
all: $(ELF) $(BIN) $(DISASM)

$(BUILD_DIR)/local/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/aimer/%.o: $(AIMER_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(ELF): $(OBJECTS) stm32f767zi.ld
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(DISASM): $(ELF)
	$(OBJDUMP) -d -S -C $< > $@

matrix:
	@for impl in reference optimized; do \
	  for param in 128f 128s 192f 192s 256f 256s; do \
	    $(MAKE) IMPL=$$impl PARAM=$$param all || exit 1; \
	  done; \
	done

clean:
	rm -rf build

-include $(DEPS)
