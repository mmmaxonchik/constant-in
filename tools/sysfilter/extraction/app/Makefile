# Makefile for egalito applications

include ../egalito/env.mk
include ../../.version-info.mk

# we want a static link of capstone
CLDFLAGS:=$(filter-out -lcapstone, $(CLDFLAGS))

CFLAGS      += -g -I ../egalito/src/ $(SYSFILTER_VERSION_FLAGS) -DSYSFILTER
CXXFLAGS    += -g -I ../egalito/src/ $(SYSFILTER_VERSION_FLAGS) -DSYSFILTER
CLDFLAGS    += -L ../egalito/src/$(BUILDDIR)

ifdef ENABLE_COVERAGE
CFLAGS 		+= -fprofile-arcs  -ftest-coverage
CXXFLAGS 	+= -fprofile-arcs  -ftest-coverage
CLDFLAGS 	+= -lgcov
endif

ifeq ($(STATIC),1)
	CLDFLAGS += -static
endif


APP_SOURCES = $(wildcard src/*.cpp)
# Sources for the shared-library wrapper (exclude sysfilter_lib.cpp from the
# main executable, and exclude the main() translation unit from the lib).
# sysfilter.cpp main() is already guarded with #ifdef SYSFILTER, so compiling
# without -DSYSFILTER keeps only the Sysfilter class.
LIB_SOURCES = $(filter-out src/sysfilter_lib.cpp, $(APP_SOURCES)) \
              src/sysfilter_lib.cpp

exe-filename = $(foreach s,$1,$(BUILDDIR)$(dir $s)$(basename $(notdir $s)))
obj-filename = $(foreach s,$1,$(BUILDDIR)$(dir $s)$(basename $(notdir $s)).o)
dep-filename = $(foreach s,$1,$(BUILDDIR)$(dir $s)$(basename $(notdir $s)).d)

ETAPP_SOURCES = $(APP_SOURCES)
ETAPP_OBJECTS = $(call obj-filename,$(ETAPP_SOURCES))

# PIC objects for the shared library (use .so naming convention from env.mk)
LIB_PIC_OBJECTS = $(patsubst %.cpp,$(BUILDDIR)%.so,$(LIB_SOURCES))

ALL_SOURCES = $(sort $(ETAPP_SOURCES) $(LIB_SOURCES))
ALL_OBJECTS = $(call obj-filename,$(ALL_SOURCES))

BUILDTREE = $(sort $(dir $(ALL_OBJECTS) $(dir $(LIB_PIC_OBJECTS))))

ETAPP     = $(BUILDDIR)sysfilter_extract
ETAPP_LIB = $(BUILDDIR)libsysfilter_analysis.so

EGALITO_PIC_A = $(EGALITO_ROOT_DIR)/src/$(BUILDDIR)libegalito_pic.a
CAPSTONE_A    = $(EGALITO_ROOT_DIR)/dep/capstone/install/lib/libcapstone.a
DISTORM_A     = $(EGALITO_ROOT_DIR)/dep/distorm3/distorm3.a

OUTPUTS = $(ETAPP) $(ETAPP_LIB)

# Default target
.PHONY: all
all: app-all .symlinks
	@true

app-all: $(OUTPUTS) | rebuild-src

ifeq ($(findstring clean,$(MAKECMDGOALS)),)
Makefile: rebuild-src
endif

$(ALL_OBJECTS): | $(BUILDTREE)
$(BUILDTREE): ../egalito/config/config.h
	@mkdir -p $@

../egalito/config/config.h:
	$(call short-make,../egalito/config)

.symlinks: $(OUTPUTS)
	@touch .symlinks
	@echo "LN-S" $(OUTPUTS)
	@ln -sf $(ETAPP)
	@ln -sf $(ETAPP_LIB)
	@ln -sf $(shell pwd)/../egalito/src/$(BUILDDIR)libegalito.so $(BUILDDIR)libegalito.so

.PHONY: rebuild-src
rebuild-src:
	$(call short-make,../egalito/src)

# Dependencies
DEPEND_FILES = $(call dep-filename,$(ALL_SOURCES))
-include $(DEPEND_FILES)

# Special files

# Programs and libraries
$(ETAPP): $(ETAPP_OBJECTS)
	$(SHORT_LINK) $(CXXFLAGS) -o $@ $^ $(CLDFLAGS) -lreadline -Wl,-rpath=$(abspath ../egalito/src)
$(ETAPP): $(EGALITO_ROOT_DIR)/src/$(BUILDDIR)libegalito.so
$(ETAPP): $(EGALITO_ROOT_DIR)/dep/distorm3/distorm3.a
$(ETAPP): $(EGALITO_ROOT_DIR)/dep/capstone/libcapstone.a

# Shared library wrapper – statically embeds egalito (PIC) + capstone so that
# the two egalito versions (sysfilter vs syspart) do not conflict at runtime.
# The version script restricts exported symbols to the C interface only.
$(ETAPP_LIB): $(LIB_PIC_OBJECTS) $(EGALITO_PIC_A) $(CAPSTONE_A) $(DISTORM_A)
	$(SHORT_LINK) -shared -fPIC \
		-Wl,--version-script=$(abspath sysfilter_lib.version) \
		$(LIB_PIC_OBJECTS) \
		-Wl,--whole-archive $(EGALITO_PIC_A) -Wl,--no-whole-archive \
		-Wl,--whole-archive $(DISTORM_A) -Wl,--no-whole-archive \
		$(CAPSTONE_A) \
		-o $@ -lstdc++ -lstdc++fs -lpthread -lreadline
$(ETAPP_LIB): $(EGALITO_PIC_A) $(CAPSTONE_A) $(DISTORM_A)

# Explicit rules so Make knows how to build these prerequisites.
$(EGALITO_PIC_A):
	$(call short-make,../egalito/src,$(BUILDDIR)libegalito_pic.a)

$(CAPSTONE_A):
	$(call short-make,../egalito/dep/capstone)

# Other targets
.PHONY: clean realclean
clean:
	-rm -rf $(BUILDDIR) .symlinks etapp
.PHONY: reformat
reformat:
	clang-format -i $(wildcard src/*.cpp) $(wildcard src/*.h)
