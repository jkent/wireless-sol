-include Makefile.local

export OPENSDK
export PATH

#SPI flash size, in K
ESP_SPI_FLASH_SIZE_K ?= 512
#0: QIO, 1: QOUT, 2: DIO, 3:DOUT
ESP_FLASH_MODE ?= 0
#0: 40MHz, 1: 26MHz, 2: 20MHz, 0xf: 80MHz
ESP_FLASH_FREQ_DIV ?= 0

# Output directors to store intermediate compiled files relative to the
# project directory
BUILD_BASE	= build
FW_BASE		= firmware

# Base directory for the compiler. Needs a / at the end; if not set it'll use
# the tools that are in the PATH.
XTENSA_TOOLS_ROOT ?=
export XTENSA_TOOLS_ROOT

# base directory of the ESP8266 SDK package, absolute
SDK_BASE	?= $(OPENSDK)/sdk
export SDK_BASE

# esptool.py path and settings
ESPTOOL		?= $(XTENSA_TOOLS_ROOT)esptool.py
ESPPORT		?= /dev/ttyUSB0
ESPBAUD		?= 460800

# using opensdk
USE_OPENSDK	?= 1
export USE_OPENSDK

# name for the target project
TARGET		= wireless-sol

# which modules (subdirectories) of the project to include in compiling
MODULES		= etslib src
EXTRA_INCDIR	= libesphttpd/include

# libraries used in this project, mainly provided by the SDK
LIBS		= c gcc hal phy pp net80211 lwip crypto wpa main
LIBS		+= esphttpd webpages-espfs

# compiler flags using during compilation of source files
CFLAGS		= -Os -ggdb -std=gnu99 -Werror -Wpointer-arith -Wundef -Wall \
		-Wl,-EL -fno-inline-functions -nostdlib -mlongcalls \
		-mtext-section-literals  -D__ets__ -DICACHE_FLASH \
		-Wno-address

ifeq ($(USE_OPENSDK),1)
CFLAGS		+= -DUSE_OPENSDK
else
CFLAGS		+= -D_STDINT_H
endif

# linker flags used to generate the main object file
LDFLAGS		= -nostdlib -Wl,--no-check-sections -u call_user_start \
		-Wl,-static

# linker script used for the above linkier step
LD_SCRIPT	= eagle.app.v6.ld

# various paths from the SDK used in this project
SDK_LIBDIR	= lib
SDK_LDDIR	= ld
SDK_INCDIR	= include include/json

# select which tools to use as compiler, librarian and linker
CC		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
AR		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-ar
LD		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc


####
#### no user configurable options below here
####
SRC_DIR		:= $(MODULES)
BUILD_DIR	:= $(addprefix $(BUILD_BASE)/,$(MODULES))

SDK_LIBDIR	:= $(addprefix $(SDK_BASE)/,$(SDK_LIBDIR))
SDK_INCDIR	:= $(addprefix -I$(SDK_BASE)/,$(SDK_INCDIR))

SRC		:= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
OBJ		:= $(patsubst %.c,$(BUILD_BASE)/%.o,$(SRC))
LIBS		:= $(addprefix -l,$(LIBS))
APP_AR		:= $(addprefix $(BUILD_BASE)/,$(TARGET)_app.a)
TARGET_OUT	:= $(addprefix $(BUILD_BASE)/,$(TARGET).out)

LD_SCRIPT	:= $(addprefix -T$(SDK_BASE)/$(SDK_LDDIR)/,$(LD_SCRIPT))

INCDIR		:= $(addprefix -I,$(SRC_DIR))
EXTRA_INCDIR	:= $(addprefix -I,$(EXTRA_INCDIR))
MODULE_INCDIR	:= $(addsuffix /include,$(INCDIR))

define maplookup
$(patsubst $(strip $(1)):%,%,$(filter $(strip $(1)):%,$(2)))
endef

ESP_FLASH_SIZE_IX=$(call maplookup,$(ESP_SPI_FLASH_SIZE_K),512:0 1024:2 2048:5 4096:6)
ESPTOOL_FREQ=$(call maplookup,$(ESP_FLASH_FREQ_DIV),0:40m 1:26m 2:20m 0xf:80m)
ESPTOOL_MODE=$(call maplookup,$(ESP_FLASH_MODE),0:qio 1:qout 2:dio 3:dout)
ESPTOOL_SIZE=$(call maplookup,$(ESP_SPI_FLASH_SIZE_K),512:4m 256:2m 1024:8m 2048:16m 4096:32m)

ESPTOOL_OPTS=--port $(ESPPORT) --baud $(ESPBAUD)
ESPTOOL_FLASHDEF=--flash_freq $(ESPTOOL_FREQ) --flash_mode $(ESPTOOL_MODE) --flash_size $(ESPTOOL_SIZE)

INCLUDE         := $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR)


V ?= $(VERBOSE)
ifeq ("$(V)","1")
Q :=
vecho := @true
else
Q := @
vecho := @echo
endif

vpath %.c $(SRC_DIR)

define compile-objects
$1/%.o: %.c
	$(vecho) "CC $$<"
	$(Q) mkdir -p $$(shell dirname $$@)
	$(Q) $(CC) $(INCLUDE) $(CFLAGS) -MMD -MF $$(@:.o=.d) -c $$< -o $$@
endef

.PHONY: all flash blankflash eraseflash run clean libesphttpd ldscript_memspecific.ld

all: $(TARGET_OUT) $(FW_BASE)

libesphttpd/Makefile:
	$(Q) echo "No libesphttpd submodule found. Using git to fetch it..."
	$(Q) git submodule init
	$(Q) git submodule update

libesphttpd: libesphttpd/Makefile
	$(Q) make -C libesphttpd

$(APP_AR): libesphttpd $(OBJ)
	$(vecho) "AR $@"
	$(Q) $(AR) cru $@ $(OBJ)

build/memspecific.ld:
	$(vecho) "GEN $@"
	$(Q) echo "MEMORY { irom0_0_seg : org = 0x40240000, len = "$$(printf "0x%X" $$(($(ESP_SPI_FLASH_SIZE)-0x4000)))" }" > $@

$(TARGET_OUT): $(APP_AR) build/memspecific.ld
	$(vecho) "LD $@"
	$(Q) $(LD) -Llibesphttpd -L$(SDK_LIBDIR) $(LD_SCRIPT) \
		build/memspecific.ld $(LDFLAGS) -Wl,--start-group $(LIBS) \
		$(APP_AR) -Wl,--end-group -o $@

$(FW_BASE): $(TARGET_OUT)
	$(vecho) "FW $@"
	$(Q) mkdir -p $@
	$(Q) $(ESPTOOL) elf2image $(TARGET_OUT) --output $@/

$(BUILD_BASE)/.flash: $(TARGET_OUT) $(FW_BASE)
	$(Q) $(ESPTOOL) $(ESPTOOL_OPTS) write_flash $(ESPTOOL_FLASHDEF) \
		0x00000 $(FW_BASE)/0x00000.bin 0x40000 $(FW_BASE)/0x40000.bin
	$(Q) touch $@

flash: $(BUILD_BASE)/.flash

blankflash:
	$(Q) $(ESPTOOL) $(ESPTOOL_OPTS) write_flash $(ESPTOOL_FLASHDEF) \
		0x7E000 $(SDK_BASE)/bin/blank.bin

eraseflash:
	$(Q) $(ESPTOOL) $(ESPTOOL_OPTS) erase_flash
	$(Q) rm -f $(BUILD_BASE)/.flash

run: flash
	$(Q) $(ESPTOOL) --port $(ESPPORT) --baud 115200 run --terminal

clean:
	$(Q) make -C libesphttpd clean
	$(Q) rm -f $(APP_AR)
	$(Q) rm -f $(TARGET_OUT)
	$(Q) rm -rf $(BUILD_BASE)
	$(Q) rm -rf $(FW_BASE)

$(foreach bdir,$(BUILD_DIR),$(eval $(call compile-objects,$(bdir))))

-include $(filter %.d,$(OBJ:%.o=%.d))
