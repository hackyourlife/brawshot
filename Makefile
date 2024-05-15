#!/bin/make
export	PREFIX	:=	
export	CC	:=	$(PREFIX)gcc
export	CXX	:=	$(PREFIX)g++
export	LD	:=	$(CXX)
export	OBJCOPY	:=	$(PREFIX)objcopy
export	NM	:=	$(PREFIX)nm
export	SIZE	:=	$(PREFIX)size
export	BIN2O	:=	bin2o
export	GLSLANG	:=	glslang

#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------
TARGET		:=	brawshot
INCLUDES	:=	include
SOURCES		:=	src
GLSLSOURCES	:=	glsl
BUILD		:=	build

ASAN		:=	#-fsanitize=address
OPTFLAGS	:=	-O3 -g
DEFINES		:=	-DUNIX -DGL_GLEXT_PROTOTYPES

CFLAGS		:=	$(OPTFLAGS) -Wall -std=c99 \
			-ffunction-sections -fdata-sections \
			$(INCLUDE) $(DEFINES) $(ASAN)

CXXFLAGS	:=	$(OPTFLAGS) -Wall -std=c++11 \
			-ffunction-sections -fdata-sections \
			$(INCLUDE) $(DEFINES) $(ASAN)

LDFLAGS		:=	$(OPTFLAGS) -Wl,-x -Wl,--gc-sections $(ASAN)

LIBS		:=	-lGL -lEGL -lturbojpeg

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CXXFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
GLSLFILES	:=	$(foreach dir,$(GLSLSOURCES),$(notdir $(wildcard $(dir)/*.glsl)))

ifneq ($(BUILD),$(notdir $(CURDIR)))
#-------------------------------------------------------------------------------
export	DEPSDIR	:=	$(CURDIR)/$(BUILD)
export	OFILES	:=	$(CFILES:.c=.o) $(CXXFILES:.cpp=.o) $(GLSLFILES:.glsl=.o) \
			BlackmagicRawAPIDispatch.o
export	VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(GLSLSOURCES),$(CURDIR)/$(dir)) $(CURDIR)
export	INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
				-I$(CURDIR)/$(BUILD) \
				-I/usr/lib/blackmagic/BlackmagicRAWSDK/Linux/Include
export	OUTPUT	:=	$(CURDIR)/$(TARGET)

.PHONY: $(BUILD) clean all

$(BUILD):
	@echo compiling...
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo "[CLEAN]"
	@rm -rf $(BUILD) $(TFILES) $(OFILES) demo

$(TARGET): $(TFILES)

else

#-------------------------------------------------------------------------------
# main target
#-------------------------------------------------------------------------------
.PHONY: all

all: $(OUTPUT)

$(OUTPUT): $(TARGET).elf
	@cp $(TARGET).elf $(OUTPUT)

BlackmagicRawAPIDispatch.o: /usr/lib/blackmagic/BlackmagicRAWSDK/Linux/Include/BlackmagicRawAPIDispatch.cpp
	@echo "[CXX]   $(notdir $@)"
	@$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) -c $< -o $@

%.o: %.c
	@echo "[CC]    $(notdir $@)"
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) -c $< -o $@
	@#$(CC) -S $(CFLAGS) -o $(@:.o=.s) $< # create assembly file

%.o: %.cpp
	@echo "[CXX]   $(notdir $@)"
	@$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) -c $< -o $@
	@#$(CPP) -S $(CFLAGS) -o $(@:.o=.s) $< # create assembly file

%.o: %.glsl
	@echo "[GLSL]  $(notdir $@)"
	@$(GLSLANG) $<
	@$(BIN2O) -t -l$(subst .,_,$(basename $@)) -i$< -o$@

$(TARGET).elf: $(OFILES)
	@echo "[LD]    $(notdir $@)"
	@$(LD) $(LDFLAGS) $(OFILES) -o $@ -Wl,-Map=$(@:.elf=.map) $(LIBS)

-include $(DEPSDIR)/*.d

#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------
