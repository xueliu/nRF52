PROJECT_NAME := iot_ipv6_coap_client_pca10040

export OUTPUT_FILENAME
#MAKEFILE_NAME := $(CURDIR)/$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))
MAKEFILE_NAME := $(MAKEFILE_LIST)
MAKEFILE_DIR := $(dir $(MAKEFILE_NAME) ) 

TEMPLATE_PATH = ../../../../../../../../components/toolchain/gcc
ifeq ($(OS),Windows_NT)
include $(TEMPLATE_PATH)/Makefile.windows
else
include $(TEMPLATE_PATH)/Makefile.posix
endif

MK := mkdir
RM := rm -rf

#echo suspend
ifeq ("$(VERBOSE)","1")
NO_ECHO := 
else
NO_ECHO := @
endif

# Toolchain commands
CC              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-gcc'
AS              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-as'
AR              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-ar' -r
LD              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-ld'
NM              := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-nm'
OBJDUMP         := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-objdump'
OBJCOPY         := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-objcopy'
SIZE            := '$(GNU_INSTALL_ROOT)/bin/$(GNU_PREFIX)-size'

#function for removing duplicates in a list
remduplicates = $(strip $(if $1,$(firstword $1) $(call remduplicates,$(filter-out $(firstword $1),$1))))

#source common to all targets
C_SOURCE_FILES += \
$(abspath ../../../../../../../../components/libraries/button/app_button.c) \
$(abspath ../../../../../../../../components/libraries/util/app_error.c) \
$(abspath ../../../../../../../../components/libraries/fifo/app_fifo.c) \
$(abspath ../../../../../../../../components/libraries/timer/app_timer.c) \
$(abspath ../../../../../../../../components/libraries/trace/app_trace.c) \
$(abspath ../../../../../../../../components/libraries/mem_manager/mem_manager.c) \
$(abspath ../../../../../../../../components/libraries/util/nrf_assert.c) \
$(abspath ../../../../../../../../components/libraries/uart/retarget.c) \
$(abspath ../../../../../../../../components/libraries/uart/app_uart_fifo.c) \
$(abspath ../../../../../../../../components/drivers_nrf/delay/nrf_delay.c) \
$(abspath ../../../../../../../../components/drivers_nrf/common/nrf_drv_common.c) \
$(abspath ../../../../../../../../components/drivers_nrf/gpiote/nrf_drv_gpiote.c) \
$(abspath ../../../../../../../../components/drivers_nrf/uart/nrf_drv_uart.c) \
$(abspath ../../../../../../../../components/drivers_nrf/pstorage/pstorage.c) \
$(abspath ../../../../../../../bsp/bsp.c) \
$(abspath ../../../main.c) \
$(abspath ../../../../../../../../components/ble/common/ble_advdata.c) \
$(abspath ../../../../../../../../components/ble/common/ble_conn_params.c) \
$(abspath ../../../../../../../../components/ble/common/ble_srv_common.c) \
$(abspath ../../../../../../../../components/iot/coap/coap.c) \
$(abspath ../../../../../../../../components/iot/coap/coap_message.c) \
$(abspath ../../../../../../../../components/iot/coap/coap_option.c) \
$(abspath ../../../../../../../../components/iot/coap/coap_queue.c) \
$(abspath ../../../../../../../../components/iot/coap/coap_resource.c) \
$(abspath ../../../../../../../../components/iot/coap/coap_transport_ipv6.c) \
$(abspath ../../../../../../../../components/iot/ipv6_stack/icmp6/icmp6.c) \
$(abspath ../../../../../../../../components/iot/context_manager/iot_context_manager.c) \
$(abspath ../../../../../../../../components/iot/ipv6_stack/pbuffer/iot_pbuffer.c) \
$(abspath ../../../../../../../../components/iot/iot_timer/iot_timer.c) \
$(abspath ../../../../../../../../components/iot/ipv6_stack/ipv6/ipv6.c) \
$(abspath ../../../../../../../../components/iot/medium/ipv6_medium_ble.c) \
$(abspath ../../../../../../../../components/iot/common/ipv6_parse.c) \
$(abspath ../../../../../../../../components/iot/ipv6_stack/utils/ipv6_utils.c) \
$(abspath ../../../../../../../../components/iot/ipv6_stack/udp/udp6.c) \
$(abspath ../../../../../../../../components/toolchain/system_nrf52.c) \
$(abspath ../../../../../../../../components/iot/medium/ble_ncfgs/ble_ncfgs.c) \
$(abspath ../../../../../../../../components/iot/medium/commissioning/commissioning.c) \
$(abspath ../../../../../../../../components/softdevice/common/softdevice_handler/softdevice_handler.c) \

#assembly files common to all targets
ASM_SOURCE_FILES  = $(abspath ../../../../../../../../components/toolchain/gcc/gcc_startup_nrf52.s)

#assembly files common to all targets
LIBS  = $(abspath ../../../../../../../../components/iot/ble_6lowpan/lib/ble_6lowpan.a)

#includes common to all targets
INC_PATHS  = -I$(abspath ../../../config)
INC_PATHS += -I$(abspath ../../../../../../../../components/iot/ipv6_stack/include)
INC_PATHS += -I$(abspath ../../../../../../../../components/iot/ipv6_stack/pbuffer)
INC_PATHS += -I$(abspath ../../../../../../../../components/iot/medium/ble_ncfgs)
INC_PATHS += -I$(abspath ../../..)
INC_PATHS += -I$(abspath ../../../../../../../../components/libraries/trace)
INC_PATHS += -I$(abspath ../../../../../../../../components/iot/ipv6_stack/udp)
INC_PATHS += -I$(abspath ../../../../../../../../components/iot/include)
INC_PATHS += -I$(abspath ../../../../../../../../components/toolchain/gcc)
INC_PATHS += -I$(abspath ../../../../../../../../components/drivers_nrf/uart)
INC_PATHS += -I$(abspath ../../../../../../../../components/device)
INC_PATHS += -I$(abspath ../../../../../../../../components/drivers_nrf/hal)
INC_PATHS += -I$(abspath ../../../../../../../bsp)
INC_PATHS += -I$(abspath ../../../../../../../../components/iot/medium/commissioning)
INC_PATHS += -I$(abspath ../../../../../../../../components/libraries/fifo)
INC_PATHS += -I$(abspath ../../../../../../../../components/softdevice/s1xx_iot/headers/nrf52)
INC_PATHS += -I$(abspath ../../../../../../../../components/drivers_nrf/gpiote)
INC_PATHS += -I$(abspath ../../../../../../../../components/softdevice/s1xx_iot/headers)
INC_PATHS += -I$(abspath ../../../../../../../../components/libraries/button)
INC_PATHS += -I$(abspath ../../../../../../../../components/iot/ble_ipsp)
INC_PATHS += -I$(abspath ../../../../../../../../components/libraries/mem_manager)
INC_PATHS += -I$(abspath ../../../../../../../../components/iot/coap)
INC_PATHS += -I$(abspath ../../../../../../../../components/iot/medium/include)
INC_PATHS += -I$(abspath ../../../../../../../../components/toolchain)
INC_PATHS += -I$(abspath ../../../../../../../../components/libraries/uart)
INC_PATHS += -I$(abspath ../../../../../../../../components/ble/common)
INC_PATHS += -I$(abspath ../../../../../../../../components/iot/medium)
INC_PATHS += -I$(abspath ../../../../../../../../components/iot/ble_6lowpan)
INC_PATHS += -I$(abspath ../../../../../../../../components/libraries/util)
INC_PATHS += -I$(abspath ../../../../../../../../components/drivers_nrf/common)
INC_PATHS += -I$(abspath ../../../../../../../../components/iot/tls)
INC_PATHS += -I$(abspath ../../../../../../../../components/iot/ipv6_stack/utils)
INC_PATHS += -I$(abspath ../../../../../../../../components/softdevice/common/softdevice_handler)
INC_PATHS += -I$(abspath ../../../../../../../../components/drivers_nrf/delay)
INC_PATHS += -I$(abspath ../../../../../../../../components/drivers_nrf/pstorage)
INC_PATHS += -I$(abspath ../../../../../../../../components/drivers_nrf/config)
INC_PATHS += -I$(abspath ../../../../../../../../components/iot/ipv6_stack/icmp6)
INC_PATHS += -I$(abspath ../../../../../../../../components/iot/iot_timer)
INC_PATHS += -I$(abspath ../../../../../../../../components/iot/context_manager)
INC_PATHS += -I$(abspath ../../../../../../../../components/libraries/timer)
INC_PATHS += -I$(abspath ../../../../../../../../components/iot/common)

OBJECT_DIRECTORY = _build
LISTING_DIRECTORY = $(OBJECT_DIRECTORY)
OUTPUT_BINARY_DIRECTORY = $(OBJECT_DIRECTORY)

# Sorting removes duplicates
BUILD_DIRECTORIES := $(sort $(OBJECT_DIRECTORY) $(OUTPUT_BINARY_DIRECTORY) $(LISTING_DIRECTORY) )

#flags common to all targets
CFLAGS  = -D__HEAP_SIZE=512
CFLAGS += -DSWI_DISABLE0
CFLAGS += -DSOFTDEVICE_PRESENT
CFLAGS += -DBOARD_PCA10040
CFLAGS += -DNRF52
CFLAGS += -DCONFIG_GPIO_AS_PINRESET
CFLAGS += -DBLE_STACK_SUPPORT_REQD
CFLAGS += -Ds1xx
CFLAGS += -mcpu=cortex-m4
CFLAGS += -mthumb -mabi=aapcs --std=gnu99
CFLAGS += -Wall -Werror -O3
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# keep every function in separate section. This will allow linker to dump unused functions
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin --short-enums
# keep every function in separate section. This will allow linker to dump unused functions
LDFLAGS += -Xlinker -Map=$(LISTING_DIRECTORY)/$(OUTPUT_FILENAME).map
LDFLAGS += -mthumb -mabi=aapcs -L $(TEMPLATE_PATH) -T$(LINKER_SCRIPT)
LDFLAGS += -mcpu=cortex-m4
LDFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# let linker to dump unused sections
LDFLAGS += -Wl,--gc-sections
# use newlib in nano version
LDFLAGS += --specs=nano.specs -lc -lnosys

# Assembler flags
ASMFLAGS += -x assembler-with-cpp
ASMFLAGS += -D__HEAP_SIZE=512
ASMFLAGS += -DSWI_DISABLE0
ASMFLAGS += -DSOFTDEVICE_PRESENT
ASMFLAGS += -DBOARD_PCA10040
ASMFLAGS += -DNRF52
ASMFLAGS += -DCONFIG_GPIO_AS_PINRESET
ASMFLAGS += -DBLE_STACK_SUPPORT_REQD
ASMFLAGS += -Ds1xx

#default target - first one defined
default: clean nrf52832_xxaa_s1xx_iot

#building all targets
all: clean
	$(NO_ECHO)$(MAKE) -f $(MAKEFILE_NAME) -C $(MAKEFILE_DIR) -e cleanobj
	$(NO_ECHO)$(MAKE) -f $(MAKEFILE_NAME) -C $(MAKEFILE_DIR) -e nrf52832_xxaa_s1xx_iot

#target for printing all targets
help:
	@echo following targets are available:
	@echo 	nrf52832_xxaa_s1xx_iot

C_SOURCE_FILE_NAMES = $(notdir $(C_SOURCE_FILES))
C_PATHS = $(call remduplicates, $(dir $(C_SOURCE_FILES) ) )
C_OBJECTS = $(addprefix $(OBJECT_DIRECTORY)/, $(C_SOURCE_FILE_NAMES:.c=.o) )

ASM_SOURCE_FILE_NAMES = $(notdir $(ASM_SOURCE_FILES))
ASM_PATHS = $(call remduplicates, $(dir $(ASM_SOURCE_FILES) ))
ASM_OBJECTS = $(addprefix $(OBJECT_DIRECTORY)/, $(ASM_SOURCE_FILE_NAMES:.s=.o) )

vpath %.c $(C_PATHS)
vpath %.s $(ASM_PATHS)

OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)

nrf52832_xxaa_s1xx_iot: OUTPUT_FILENAME := nrf52832_xxaa_s1xx_iot
nrf52832_xxaa_s1xx_iot: LINKER_SCRIPT=iot_ipv6_coap_client_gcc_nrf52.ld

nrf52832_xxaa_s1xx_iot: $(BUILD_DIRECTORIES) $(OBJECTS)
	@echo Linking target: $(OUTPUT_FILENAME).out
	$(NO_ECHO)$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	$(NO_ECHO)$(MAKE) -f $(MAKEFILE_NAME) -C $(MAKEFILE_DIR) -e finalize

## Create build directories
$(BUILD_DIRECTORIES):
	echo $(MAKEFILE_NAME)
	$(MK) $@

# Create objects from C SRC files
$(OBJECT_DIRECTORY)/%.o: %.c
	@echo Compiling file: $(notdir $<)
	$(NO_ECHO)$(CC) $(CFLAGS) $(INC_PATHS) -c -o $@ $<

# Assemble files
$(OBJECT_DIRECTORY)/%.o: %.s
	@echo Assembly file: $(notdir $<)
	$(NO_ECHO)$(CC) $(ASMFLAGS) $(INC_PATHS) -c -o $@ $<
# Link
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out: $(BUILD_DIRECTORIES) $(OBJECTS)
	@echo Linking target: $(OUTPUT_FILENAME).out
	$(NO_ECHO)$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
## Create binary .bin file from the .out file
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).bin: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	@echo Preparing: $(OUTPUT_FILENAME).bin
	$(NO_ECHO)$(OBJCOPY) -O binary $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).bin

## Create binary .hex file from the .out file
$(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex: $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	@echo Preparing: $(OUTPUT_FILENAME).hex
	$(NO_ECHO)$(OBJCOPY) -O ihex $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex

finalize: genbin genhex echosize

genbin:
	@echo Preparing: $(OUTPUT_FILENAME).bin
	$(NO_ECHO)$(OBJCOPY) -O binary $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).bin

## Create binary .hex file from the .out file
genhex: 
	@echo Preparing: $(OUTPUT_FILENAME).hex
	$(NO_ECHO)$(OBJCOPY) -O ihex $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).hex
echosize:
	-@echo ''
	$(NO_ECHO)$(SIZE) $(OUTPUT_BINARY_DIRECTORY)/$(OUTPUT_FILENAME).out
	-@echo ''

clean:
	$(RM) $(BUILD_DIRECTORIES)

cleanobj:
	$(RM) $(BUILD_DIRECTORIES)/*.o
flash: $(MAKECMDGOALS)
	@echo Flashing: $(OUTPUT_BINARY_DIRECTORY)/$<.hex
	nrfjprog --program $(OUTPUT_BINARY_DIRECTORY)/$<.hex -f nrf52 --sectorerase
	nrfjprog --reset

## Flash softdevice