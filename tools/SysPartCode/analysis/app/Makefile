# Makefile for egalito applications

include ../tools/egalito/env.mk
CXXFLAGS    += -g -I ../tools/egalito/src/ -I src/
# Build with: make ASAN=1   (adds AddressSanitizer + LeakSanitizer)
ifeq ($(ASAN),1)
CXXFLAGS    += -fsanitize=address -fsanitize=leak -fno-omit-frame-pointer
CLDFLAGS    += -fsanitize=address -fsanitize=leak
endif
CLDFLAGS    += -L ../tools/egalito/src/$(BUILDDIR) -legalito \
	-Wl,-rpath=$(abspath ../tools/egalito/src/$(BUILDDIR)) \
	-Wl,-rpath=$(abspath ../tools/egalito/dep/capstone/install/lib)

LOOP_SOURCES 		  = src/loopanalysis.cpp
TEST_LOOP_SOURCES	  = src/testLoops.cpp
SYSPART_SOURCES 	  = src/syspart.cpp src/testSyspart.cpp
NSS_SOURCES           = src/nss.cpp
IPCALLGRAPH_SOURCES   = src/ipcallgraph.cpp src/syspartUtility.cpp
NONRETURN_SOURCES 	  = src/nonreturnAnalysis.cpp
SYSPART_UTILITY_SOURCES = src/syspartUtility.cpp

# Sources for the shared-library wrapper.
# Exclude testSyspart.cpp (contains main()) and include syspart_lib.cpp.
SYSPART_LIB_SOURCES = src/syspart.cpp src/syspart_lib.cpp \
                      $(IPCALLGRAPH_SOURCES) $(LOOP_SOURCES) \
                      $(NONRETURN_SOURCES) $(NSS_SOURCES)

exe-filename = $(foreach s,$1,$(BUILDDIR)$(dir $s)$(basename $(notdir $s)))
obj-filename = $(foreach s,$1,$(BUILDDIR)$(dir $s)$(basename $(notdir $s)).o)
dep-filename = $(foreach s,$1,$(BUILDDIR)$(dir $s)$(basename $(notdir $s)).d)

ETLOOP_SOURCES = $(TEST_LOOP_SOURCES) $(LOOP_SOURCES)
ETLOOP_OBJECTS = $(call obj-filename,$(ETLOOP_SOURCES))

ETSYSPART_SOURCES = $(SYSPART_SOURCES) $(IPCALLGRAPH_SOURCES) $(LOOP_SOURCES) $(NONRETURN_SOURCES) $(SYSPART_UTILITY_SOURCES) $(NSS_SOURCES)
ETSYSPART_OBJECTS = $(call obj-filename, $(ETSYSPART_SOURCES))

# PIC objects for the shared library
SYSPART_LIB_PIC_OBJECTS = $(patsubst %.cpp,$(BUILDDIR)%.so,$(SYSPART_LIB_SOURCES))

ALL_SOURCES = $(sort $(ETLOOP_SOURCES) $(ETSYSPART_SOURCES) $(SYSPART_LIB_SOURCES))

ALL_OBJECTS = $(call obj-filename,$(ALL_SOURCES))

BUILDTREE = $(sort $(dir $(ALL_OBJECTS) $(dir $(SYSPART_LIB_PIC_OBJECTS))))

ETLOOP		 = $(BUILDDIR)loops
ETSYSPART    = $(BUILDDIR)syspart
ETSYSPART_LIB = $(BUILDDIR)libsyspart_analysis.so

EGALITO_PIC_A = ../tools/egalito/src/$(BUILDDIR)libegalito_pic.a
CAPSTONE_A    = ../tools/egalito/dep/capstone/install/lib/libcapstone.a
DISTORM_A     = ../tools/egalito/dep/distorm3/distorm3.a

OUTPUTS = $(ETLOOP) $(ETSYSPART) $(ETSYSPART_LIB)

# Default target
.PHONY: all
all: app-all .symlinks
	@true

app-all: $(OUTPUTS) | rebuild-src

ifeq ($(findstring clean,$(MAKECMDGOALS)),)
Makefile: rebuild-src
endif

$(ALL_OBJECTS): | $(BUILDTREE)
$(BUILDTREE): ../tools/egalito/config/config.h
	@mkdir -p $@

../config/config.h:
	$(call short-make,../tools/egalito/config)

.symlinks: $(OUTPUTS)
	@touch .symlinks
	@ln -sf $(ETLOOP)
	@ln -sf $(ETSYSPART)
	@ln -sf $(ETSYSPART_LIB)
	@cp $(shell pwd)/../tools/egalito/src/$(BUILDDIR)libegalito.so $(BUILDDIR)

.PHONY: rebuild-src
rebuild-src:
	$(call short-make,../tools/egalito/src)

# Dependencies
DEPEND_FILES = $(call dep-filename,$(ALL_SOURCES))
-include $(DEPEND_FILES)

# Programs and libraries

$(ETLOOP): $(ETLOOP_OBJECTS)
	$(SHORT_LINK) $(CXXFLAGS) -o $@ $^ $(CLDFLAGS) -Wl,-rpath=$(abspath ../tools/egalito/src)
$(ETLOOP): ../tools/egalito/src/$(BUILDDIR)libegalito.so

$(ETSYSPART): $(ETSYSPART_OBJECTS)
	$(SHORT_LINK) $(CXXFLAGS) -o $@ $^ $(CLDFLAGS) -Wl,-rpath=$(abspath ../tools/egalito/src)
$(ETSYSPART): ../tools/egalito/src/$(BUILDDIR)libegalito.so

# Shared library wrapper – statically embeds egalito (PIC) + capstone so that
# the two egalito versions (sysfilter vs syspart) do not conflict at runtime.
# The version script restricts exported symbols to the C interface only.
$(ETSYSPART_LIB): $(SYSPART_LIB_PIC_OBJECTS) $(EGALITO_PIC_A) $(CAPSTONE_A) $(DISTORM_A)
	$(SHORT_LINK) -shared -fPIC \
		-Wl,--version-script=$(abspath syspart_lib.version) \
		$(SYSPART_LIB_PIC_OBJECTS) \
		-Wl,--whole-archive $(EGALITO_PIC_A) -Wl,--no-whole-archive \
		-Wl,--whole-archive $(DISTORM_A) -Wl,--no-whole-archive \
		$(CAPSTONE_A) \
		-o $@ -lstdc++ -lstdc++fs -lpthread -lreadline
$(ETSYSPART_LIB): $(EGALITO_PIC_A) $(CAPSTONE_A) $(DISTORM_A)

# Explicit rule so Make knows how to build libegalito_pic.a before the .so link.
# We call make in the egalito src directory with the BUILDDIR-relative target.
$(EGALITO_PIC_A):
	$(call short-make,../tools/egalito/src,$(BUILDDIR)libegalito_pic.a)

$(CAPSTONE_A):
	$(call short-make,../tools/egalito/dep/capstone)

# Other targets
.PHONY: clean realclean
clean:
	-rm -rf $(BUILDDIR) .symlinks loops syspart
