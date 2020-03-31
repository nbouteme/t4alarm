### !!! Nécessite teensyduino d'installé !!! ###
### Disponible sur l'AUR mais probablement   ###
### sur aucun autre gestionnaire de paquet   ###
### donc nécessite une installation manuelle ###

include config.mk
ARDUINOPATH = /usr/share/arduino

COREFOLDER = $(ARDUINOPATH)/hardware/teensy/avr/cores/teensy4
LIBRARIESFOLDER = $(ARDUINOPATH)/hardware/teensy/avr/libraries

MCU=IMXRT1062
TARGET = main
OPTIONS = -DF_CPU=600000000 -DUSB_SERIAL -DLAYOUT_US_ENGLISH
OPTIONS += -D__$(MCU)__ -DARDUINO=10810 -DTEENSYDUINO=149 -DARDUINO_TEENSY40
CPUOPTIONS = -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16 -mthumb
# use this for a smaller, no-float printf
#SPECS = --specs=nano.specs

# path location for Teensy Loader, teensy_post_compile and teensy_reboot (on Linux)
TOOLSPATH = $(abspath $(ARDUINOPATH)/hardware/tools)

# path location for Arduino libraries (currently not used)
LIBRARYPATH = $(abspath $(ARDUINOPATH)/libraries)

# path location for the arm-none-eabi compiler
#COMPILERPATH = $(abspath $(ARDUINOPATH)/hardware/tools/arm/bin)
COMPILERPATH = /usr/bin

LIBFLAGS = $(addprefix -I$(LIBRARIESFOLDER)/,$(ARDUINO_LIBS))
PCLIBFLAGS = $(addprefix -Ipclibs/,$(ARDUINO_LIBS))

CPPFLAGS = -Wall -Os $(CPUOPTIONS) -MMD $(OPTIONS) -I. -ffunction-sections -fdata-sections -I$(COREFOLDER)  $(LIBFLAGS) $(addsuffix /utility,$(LIBFLAGS))
PCCPPFLAGS = -fPIC -Wall -Og -MMD $(OPTIONS) -I. -ffunction-sections -fdata-sections -Ipccore  $(PCLIBFLAGS) $(addsuffix /utility,$(PCLIBFLAGS))
CXXFLAGS = -std=gnu++17 -felide-constructors -fno-exceptions -fpermissive -fno-rtti -Wno-error=narrowing
CFLAGS = 

## Normalement pas besoin de toucher plus bas ##

LDFLAGS = -Os -Wl,--gc-sections,--relax $(SPECS) $(CPUOPTIONS) -T$(MCU_LD)
LIBS = -L$(ARDUINOPATH)/hardware/tools/arm/arm-none-eabi/lib -larm_cortexM7lfsp_math -lm -lstdc++
CC = $(COMPILERPATH)/arm-none-eabi-gcc
CXX = $(COMPILERPATH)/arm-none-eabi-g++
OBJCOPY = $(COMPILERPATH)/arm-none-eabi-objcopy
SIZE = $(COMPILERPATH)/arm-none-eabi-size

C_FILES := $(shell find src/ -iname '*.c')
CPP_FILES := $(shell find src/ -iname '*.cpp')

CORE_CFILES   := $(shell find $(COREFOLDER)/ -iname '*.c')
CORE_CPPFILES := $(shell find $(COREFOLDER)/ -iname '*.cpp')

EXCL =  -not -path '*extras*' -not -path '*example*' -not -path '*doc*'
LIBS_CFILES := $(shell find $(addprefix $(LIBRARIESFOLDER)/,$(ARDUINO_LIBS)) -iname '*.c' $(EXCL))
LIBS_CPPFILES := $(shell find $(addprefix $(LIBRARIESFOLDER)/,$(ARDUINO_LIBS)) -iname '*.cpp'  $(EXCL))
LIBS_ASMFILES := $(shell find $(addprefix $(LIBRARIESFOLDER)/,$(ARDUINO_LIBS)) -iname '*.S'  $(EXCL))

ALL_FILES = $(C_FILES) $(CPP_FILES) $(CORE_CFILES) $(CORE_CPPFILES) $(LIBS_CFILES) $(LIBS_CPPFILES) $(LIBS_ASMFILES)
ALL_FOLDERS := $(sort $(dir $(ALL_FILES)))
ALL_PCFOLDERS := $(addprefix pcbin/,$(ALL_FOLDERS))
ALL_FOLDERS := $(addprefix bin/,$(ALL_FOLDERS))

define folder_rule =
$(1):
	mkdir -p $(1)
endef

OBJS := $(C_FILES:.c=.o) $(CPP_FILES:.cpp=.o)
PCOBJS := $(addprefix pcbin/, $(OBJS))
OBJS := $(addprefix bin/, $(OBJS))

COBJS := $(CORE_CFILES:.c=.o) $(CORE_CPPFILES:.cpp=.o)
COBJS := $(addprefix bin/, $(COBJS))

LOBJS := $(LIBS_CFILES:.c=.o) $(LIBS_CPPFILES:.cpp=.o) $(LIBS_ASMFILES:.S=.o)
LOBJS := $(addprefix bin/, $(LOBJS))

LIBARC = $(addsuffix .a,$(addprefix bin/,$(ARDUINO_LIBS)))

all: bin $(TARGET).hex

$(foreach folder,$(ALL_FOLDERS) $(ALL_PCFOLDERS),$(eval $(call folder_rule,$(folder))))

$(TARGET).elf: $(OBJS) $(MCU_LD) bin bin/core.a $(LIBARC)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBARC) bin/core.a $(LIBS)

emu.so: pcbin $(PCOBJS) bin
	g++ -g -fPIC -shared $(PCOBJS) -o $@ 

bin/core.a: $(COBJS)
	ar rc $@ $(COBJS)

$(LIBARC): $(LOBJS)
	ar rc $@ $(LOBJS)

bin: $(ALL_FOLDERS)

pcbin: $(ALL_PCFOLDERS)

bin/%.o: %.S
	$(CC) $< $(CPPFLAGS) $(CFLAGS) -c -o $@
bin/%.o: %.c
	$(CC) $< $(CPPFLAGS) $(CFLAGS) -c -o $@
bin/%.o: %.cpp
	$(CXX) $< $(CPPFLAGS) $(CXXFLAGS) -c -o $@

pcbin/%.o: %.c
	gcc $< $(PCCPPFLAGS) $(CFLAGS) -c -o $@
pcbin/%.o: %.cpp
	g++ $< $(PCCPPFLAGS) $(CXXFLAGS) -c -o $@

%.hex: %.elf
	$(SIZE) $<
	$(OBJCOPY) -O ihex -R .eeprom $< $@
ifneq (,$(wildcard $(TOOLSPATH)))
endif

prog: $(TARGET).hex
	$(TOOLSPATH)/teensy_post_compile -file=$(basename $(TARGET).hex) -path=$(shell pwd) -tools=$(TOOLSPATH)
	-$(TOOLSPATH)/teensy_reboot

.PHONY = prog

# compiler generated dependency info
-include $(OBJS:.o=.d)
-include $(PCOBJS:.o=.d)

# make "MCU" lower case
LOWER_MCU := $(subst A,a,$(subst B,b,$(subst C,c,$(subst D,d,$(subst E,e,$(subst F,f,$(subst G,g,$(subst H,h,$(subst I,i,$(subst J,j,$(subst K,k,$(subst L,l,$(subst M,m,$(subst N,n,$(subst O,o,$(subst P,p,$(subst Q,q,$(subst R,r,$(subst S,s,$(subst T,t,$(subst U,u,$(subst V,v,$(subst W,w,$(subst X,x,$(subst Y,y,$(subst Z,z,$(MCU)))))))))))))))))))))))))))
MCU_LD = $(LOWER_MCU).ld

clean:
	rm -rf *.o *.d $(TARGET).elf $(TARGET).hex bin
