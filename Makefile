ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO. Run: export DEVKITPRO=/opt/devkitpro")
endif

TARGET   := wire-game
BUILD    := build
SOURCES  := source
INCLUDES :=
DATA     :=

ARCH     := -marm -mthumb-interwork
CFLAGS   := -g -Wall -O2 -mcpu=arm7tdmi -mtune=arm7tdmi $(ARCH)
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS  := -g $(ARCH)
LDFLAGS  := -g $(ARCH) -Wl,-Map,$(notdir $*.map)
LIBS     := -lgba
LIBGBA   ?= $(DEVKITPRO)/libgba
LIBDIRS  := $(LIBGBA)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(CURDIR)/$(TARGET)
export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                   $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES          := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
export LD       := arm-none-eabi-gcc
export OFILES   := $(CFILES:.c=.o)
export INCLUDE  := $(foreach dir,$(INCLUDES),$(addprefix -I$(CURDIR)/,$(dir))) \
                   $(foreach dir,$(LIBDIRS),$(addprefix -I,$(dir)/include)) \
                   -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),$(addprefix -L,$(dir)/lib))

.PHONY: $(BUILD) clean

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).gba

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).gba : $(OUTPUT).elf
$(OUTPUT).elf : $(OFILES)

-include $(DEPENDS)

endif

include $(DEVKITPRO)/devkitARM/gba_rules
