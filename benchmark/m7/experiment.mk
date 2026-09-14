TOOL_ROOT ?= /private/tmp/aimer-m7-tools
PREFIX := $(TOOL_ROOT)/gcc/bin/arm-none-eabi-
CC := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump
SIZE := $(PREFIX)size

MODE ?= kernel
PARAM ?= 128f
IMPL ?= reference

ifeq ($(IMPL),reference)
AIMER_DIR := ../../Reference_Implementation
else ifeq ($(IMPL),optimized)
AIMER_DIR := ../../gf_inv_opt
else ifeq ($(IMPL),paired)
AIMER_DIR := ../../Reference_Implementation
else
$(error IMPL must be reference, optimized, or paired)
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

ifeq ($(MODE),correctness)
ifeq ($(IMPL),paired)
else
$(error correctness mode requires IMPL=paired)
endif
LOCAL_NAMES := startup platform correctness_field
BUILD_TAG := correctness-field-$(PARAM)
else ifeq ($(MODE),kernel)
LOCAL_NAMES := startup platform kernel_bench
BUILD_TAG := kernel-$(IMPL)-$(PARAM)
else ifeq ($(MODE),e2e)
LOCAL_NAMES := startup platform allocator e2e_bench
BUILD_TAG := e2e-$(IMPL)-$(PARAM)
else
$(error MODE must be correctness, kernel, or e2e)
endif

BUILD_DIR := build/$(BUILD_TAG)
ELF := $(BUILD_DIR)/$(MODE).elf
MAP := $(BUILD_DIR)/$(MODE).map
BIN := $(BUILD_DIR)/$(MODE).bin
DISASM := $(BUILD_DIR)/$(MODE).disasm.txt

CPU_FLAGS := -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
COMMON_FLAGS := -O3 -std=c11 $(CPU_FLAGS) -fno-lto -ffunction-sections \
	-fdata-sections -fstack-usage -Wall -Wextra -Wpedantic
CFLAGS := $(COMMON_FLAGS) -I$(AIMER_DIR) -DPARAMS=$(PARAM) \
	-DIMPL_NAME=\"$(IMPL)\" -MMD -MP
LDFLAGS := $(CPU_FLAGS) -fno-lto -nostartfiles --specs=nano.specs \
	-Tstm32f767zi.ld -Wl,--gc-sections,-Map,$(MAP) \
	-Wl,--print-memory-usage

ifeq ($(MODE),e2e)
ifneq (,$(filter 128f 192f,$(PARAM)))
CFLAGS += -DE2E_SIGN_VERIFY=1
else
CFLAGS += -DE2E_SIGN_VERIFY=0
endif
endif

LOCAL_OBJECTS := $(addprefix $(BUILD_DIR)/local/,$(addsuffix .o,$(LOCAL_NAMES)))

ifeq ($(MODE),correctness)
REF_OBJECTS := $(BUILD_DIR)/reference/field_common.o \
	$(BUILD_DIR)/reference/$(FIELD_SOURCE:.c=.o)
OPT_OBJECTS := $(BUILD_DIR)/optimized/field_common.o
OBJECTS := $(LOCAL_OBJECTS) $(REF_OBJECTS) $(OPT_OBJECTS)
NAMESPACE_PREFIX := samsungsds_aimer_$(PARAM)_ref_
RENAMED_COMMON_FUNCTIONS := gf_to_bytes gf_from_bytes gf_set0 gf_is0 gf_copy \
	gf_add gf_inv gf_mul_add gf_mat_vec_mul_add
OPT_RENAME_FLAGS := $(foreach function,$(RENAMED_COMMON_FUNCTIONS), \
	-D$(NAMESPACE_PREFIX)$(function)=m7_opt_$(function))
else ifeq ($(MODE),kernel)
AIMER_OBJECTS := $(BUILD_DIR)/aimer/field_common.o \
	$(BUILD_DIR)/aimer/$(FIELD_SOURCE:.c=.o)
OBJECTS := $(LOCAL_OBJECTS) $(AIMER_OBJECTS)
else
AIMER_NAMES := aim3 field_common hash sign tree common/aes common/fips202 common/rng
AIMER_OBJECTS := $(addprefix $(BUILD_DIR)/aimer/,$(addsuffix .o,$(AIMER_NAMES))) \
	$(BUILD_DIR)/aimer/$(FIELD_SOURCE:.c=.o)
OBJECTS := $(LOCAL_OBJECTS) $(AIMER_OBJECTS)
endif

DEPS := $(OBJECTS:.o=.d)

.PHONY: all correctness-matrix kernel-matrix e2e-matrix experiment-matrix
all: $(ELF) $(BIN) $(DISASM)

$(BUILD_DIR)/local/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/aimer/%.o: $(AIMER_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/reference/%.o: ../../Reference_Implementation/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/optimized/field_common.o: ../../gf_inv_opt/field_common.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I../../gf_inv_opt $(OPT_RENAME_FLAGS) -c $< -o $@

$(ELF): $(OBJECTS) stm32f767zi.ld
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(DISASM): $(ELF)
	$(OBJDUMP) -d -S -C $< > $@

correctness-matrix:
	@for field in 128f 192f 256f; do \
	  $(MAKE) -f experiment.mk MODE=correctness IMPL=paired PARAM=$$field all || exit 1; \
	done

kernel-matrix:
	@for impl in reference optimized; do \
	  for field in 128f 192f 256f; do \
	    $(MAKE) -f experiment.mk MODE=kernel IMPL=$$impl PARAM=$$field all || exit 1; \
	  done; \
	done

e2e-matrix:
	@for impl in reference optimized; do \
	  for param in 128f 128s 192f 192s 256f 256s; do \
	    $(MAKE) -f experiment.mk MODE=e2e IMPL=$$impl PARAM=$$param all || exit 1; \
	  done; \
	done

experiment-matrix: correctness-matrix kernel-matrix e2e-matrix

-include $(DEPS)
