# Makefile for CAN_XR software
#
# ---
# Common definitions
# ---

# Host C compiler, CDEFS, and CFLAGS.
CC = cc -std=c99 -Wall
CDEFS =
CINCS = -I$(HOST_INCDIR) -I$(CAN_XR_INCDIR)
CFLAGS = $(CDEFS) $(CINCS)
AR = ar

# Cross-compilation toolchain
XARCH = arm-none-eabi-
XCC = $(XARCH)gcc -std=c99 -Wall
XAR = $(XARCH)ar
XOBJCOPY = $(XARCH)objcopy

# Compiler driver specs for cross-compiling, set as required by the
# toolchain in use.
XSPECS =

# Linker script for cross-compiling, set as required by the toolchain
# in use.
XLDSCRIPT =

# Additional libraries for cross-compiling, set as required by the
# toolchain in use.
XLDLIBS =


# Compiler flags when cross-compling
#
# -DENABLE_TRACE enables TRACE()
#
XCDEFS = -mthumb -mcpu=cortex-m3 -O4 -specs=$(XSPECS)

XCINCS = -I$(CROSS_INCDIR) -I$(CAN_XR_INCDIR)

XCFLAGS = $(XCDEFS) $(XCINCS)

# Linker flags when cross compiling
XLDFLAGS = -T$(XLDSCRIPT) \
-Wl,-Map=$(patsubst %.elf,%.map,$@),--cref


# Sources and includes of the platform-independent part of the controller.
CAN_XR_SRCDIR = CAN_XR_Controller
CAN_XR_INCDIR = $(CAN_XR_SRCDIR)/include
CAN_XR_SRCS   = $(wildcard $(CAN_XR_SRCDIR)/*.c)

# Default target.
all: host-all host-tests all-cross TAGS

# Other Common targets.
clean: host-clean clean-cross

# Generate TAGS file for emacs.
.PHONY: TAGS
TAGS:
	etags `find . -name '*.[ch]' -print`

# ---
# Host section
# ---

# Sources and includes of the host-dependent part of the controller.
HOST_SRCDIR = Host
HOST_INCDIR = $(HOST_SRCDIR)/include
HOST_SRCS   = $(wildcard $(HOST_SRCDIR)/*.c)

# Object directory for host builds.
HOST_OBJDIR = Host

# Platform-independent and host-dependent objects files,
# when building on the host.
HOST_CAN_XR_OBJS = $(CAN_XR_SRCS:$(CAN_XR_SRCDIR)/%.c=$(HOST_OBJDIR)/%.o)
HOST_OBJS = $(HOST_SRCS:$(HOST_SRCDIR)/%.c=$(HOST_OBJDIR)/%.o)

# All objects to build on the host and corresponding deps files.
HOST_ALL_OBJS = $(HOST_CAN_XR_OBJS) $(HOST_OBJS)
HOST_ALL_DEPS = $(HOST_ALL_OBJS:%.o=%.d)

# Host library.
HOST_LIB = $(HOST_OBJDIR)/libcan_xr.a

# Rules to build .d files on the host.
# We need two rules because we have two source directories.
$(HOST_OBJDIR)/%.d: $(CAN_XR_SRCDIR)/%.c
	@echo "Generating $@"
	@set +e; $(CC) -MM $(CFLAGS) $< \
	| sed "s,.*\.o,$(patsubst %.d,%.o,$@) $@," \
	> $@; \
	[ -s $@ ] || rm -f $@

$(HOST_OBJDIR)/%.d: $(HOST_SRCDIR)/%.c
	@echo "Generating $@"
	@set +e; $(CC) -MM $(CFLAGS) $< \
	| sed "s,.*\.o,$(patsubst %.d,%.o,$@) $@," \
	> $@; \
	[ -s $@ ] || rm -f $@

-include $(HOST_ALL_DEPS)

# Rules to build .o files on the host.
$(HOST_OBJDIR)/%.o: $(CAN_XR_SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(HOST_OBJDIR)/%.o: $(HOST_SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Finally, build the host library.
$(HOST_LIB): $(HOST_ALL_OBJS)
	$(AR) rs $@ $?

# Host programs.
HOST_PROGRAMS_SRCS = $(wildcard Host_Programs/*.c)
HOST_PROGRAMS_EXEC = $(HOST_PROGRAMS_SRCS:%.c=%)
HOST_PROGRAMS_DEPS = $(HOST_PROGRAMS_SRCS:%.c=%.d)

Host_Programs/%: Host_Programs/%.c $(HOST_LIB)
	$(CC) $(CFLAGS) -o $@ $< $(HOST_LIB)

Host_Programs/%.d: Host_Programs/%.c
	@echo "Generating $@"
	@set +e; $(CC) -MM $(CFLAGS) $< \
	| sed "s,.*\.o,$(patsubst %.d,%,$@) $@," \
	> $@; \
	[ -s $@ ] || rm -f $@

-include $(HOST_PROGRAMS_DEPS)

# Host targets.
host-all: $(HOST_LIB) $(HOST_PROGRAMS_EXEC)

host-clean:
	rm -f $(HOST_LIB) $(HOST_ALL_OBJS) $(HOST_ALL_DEPS) \
	$(HOST_PROGRAMS_EXEC) $(HOST_PROGRAMS_DEPS)

# Host tests

# Input (.txt) and output (.dat) files in the 01 test group.

HOST_INPUTS_01  = $(wildcard Host_Tests/Inputs/01*.txt)
HOST_DATA_01    = $(patsubst Host_Tests/Inputs/%.txt, \
	Host_Tests/Results/%.dat,$(HOST_INPUTS_01))

# Rule to build .dat files in the 01 test group.

Host_Tests/Results/01%.dat: Host_Tests/Inputs/01%.txt $(HOST_PROGRAMS_EXEC)
	Host_Programs/01* <$< \
	>$@ \
	2>$(@:%.dat=%.log)

# Process .dat files in the 01 test group with gnuplot 01.gnuplot to
# produce PDF output.

HOST_PDF_01     = $(HOST_DATA_01:%.dat=%.pdf)

Host_Tests/Results/01%.pdf: Host_Tests/Results/01%.dat \
	Host_Tests/Inputs/01.gnuplot
	gnuplot -e "output_file=\"$@\"" \
	-e "input_file=\"$<\"" \
	Host_Tests/Inputs/01.gnuplot

# Keep adding as more test groups come out

HOST_PDF  = $(HOST_PDF_01)


host-tests: host-all $(HOST_PDF)


# ---
# Cross-compilation section
# ---

# Sources and includes of the board-dependent part of the controller.
CROSS_SRCDIR = Cross
CROSS_INCDIR = $(CROSS_SRCDIR)/include
CROSS_SRCS   = $(wildcard $(CROSS_SRCDIR)/*.c)

# Object directory for cross-compilation.
CROSS_OBJDIR = Cross

# Platform-independent and board-dependent objects files,
# when building for the board.
CROSS_CAN_XR_OBJS = $(CAN_XR_SRCS:$(CAN_XR_SRCDIR)/%.c=$(CROSS_OBJDIR)/%.o)
CROSS_OBJS = $(CROSS_SRCS:$(CROSS_SRCDIR)/%.c=$(CROSS_OBJDIR)/%.o)

# All objects to build on the board and corresponding deps files.
CROSS_ALL_OBJS = $(CROSS_CAN_XR_OBJS) $(CROSS_OBJS)
CROSS_ALL_DEPS = $(CROSS_ALL_OBJS:%.o=%.d)

# Cross-compiled library.
CROSS_LIB = $(CROSS_OBJDIR)/libcan_xr.a

# Dependencies

$(CROSS_OBJDIR)/%.d: $(CAN_XR_SRCDIR)/%.c $(XSPECS)
	@echo "Generating $@"
	@set +e; \
	mkdir -p `dirname $@`; \
	$(XCC) -MM $(XCFLAGS) $< \
	| sed "s,.*\.o,$(patsubst %.d,%.o,$@) $@," \
	> $@; \
	[ -s $@ ] || rm -f $@

$(CROSS_OBJDIR)/%.d: $(CROSS_SRCDIR)/%.c $(XSPECS)
	@echo "Generating $@"
	@set +e; \
	mkdir -p `dirname $@`; \
	$(XCC) -MM $(XCFLAGS) $< \
	| sed "s,.*\.o,$(patsubst %.d,%.o,$@) $@," \
	> $@; \
	[ -s $@ ] || rm -f $@

-include $(CROSS_ALL_DEPS)

# Rule to cross-build object files
$(CROSS_OBJDIR)/%.o: $(CAN_XR_SRCDIR)/%.c $(XSPECS)
	$(XCC) -c $(XCFLAGS) -o $@ $<

$(CROSS_OBJDIR)/%.o: $(CROSS_SRCDIR)/%.c $(XSPECS)
	$(XCC) -c $(XCFLAGS) -o $@ $<

# Rule to build the cross-compiled library
$(CROSS_LIB): $(CROSS_ALL_OBJS)
	$(XAR) rs $@ $?

# Host programs.
CROSS_PROGRAMS_SRCS = $(wildcard Cross_Programs/*.c)
CROSS_PROGRAMS_EXEC = $(CROSS_PROGRAMS_SRCS:%.c=%.elf)
CROSS_PROGRAMS_HEX  = $(CROSS_PROGRAMS_SRCS:%.c=%.hex)
CROSS_PROGRAMS_DEPS = $(CROSS_PROGRAMS_SRCS:%.c=%.d)

Cross_Programs/%.elf: Cross_Programs/%.c $(XSPECS) $(XLDSCRIPT) \
	$(CROSS_LIB)
	$(XCC) $(XCFLAGS) -o $@ $(XLDFLAGS) $< $(CROSS_LIB) $(XLDLIBS)

Cross_Programs/%.d: Cross_Programs/%.c $(XSPECS)
	@echo "Generating $@"
	@set +e; $(XCC) -MM $(XCFLAGS) $< \
	| sed "s,.*\.o,$(patsubst %.d,%.elf,$@) $@," \
	> $@; \
	[ -s $@ ] || rm -f $@

# This rule should only be used when targeting the .o
# of main programs explicitly, for instance, for footprint
Cross_Programs/%.o: Cross_Programs/%.c $(XSPECS) $(XLDSCRIPT) \
	$(CROSS_LIB)
	@echo "Generating $@ upon request"
	$(XCC) -c $(XCFLAGS) -o $@ $(XLDFLAGS) $< $(CROSS_LIB) $(XLDLIBS)

-include $(CROSS_PROGRAMS_DEPS)


###
#
# Conversion to Intel HEX format for lpc21isp
#
# To check whether the link to the board is good:
#  $ lpc21isp -control -detectonly /dev/null /dev/ttyUSB0 115200 12000
#
# To reset the board, start the program that's currently on flash, and
# look at the console output:
#  $ lpc21isp -control -termonly /dev/null /dev/ttyUSB0 115200 12000
#
# To upload <something> into the board and start it:
#  $ lpc21isp -term -control <something>.hex /dev/ttyUSB0 115200 12000
#
# The 'naked' programmer needs the -controlinv option, the 'grey' one
# does not.
###

Cross_Programs/%.hex: Cross_Programs/%.elf
	$(XOBJCOPY) -O ihex $< $@

all-cross: $(CROSS_LIB) $(CROSS_PROGRAMS_HEX)

clean-cross:
	rm -f $(CROSS_LIB) $(CROSS_ALL_OBJS) $(CROSS_ALL_DEPS) \
	$(CROSS_PROGRAMS_EXEC) $(CROSS_PROGRAMS_HEX) $(CROSS_PROGRAMS_DEPS)

# Build distribution package

DIST_TMPDIR=.
DIST_ROOT=SDCC_Release

DIST_LIST=Makefile README.txt LICENSE CAN_XR_Controller Cross Host \
Cross_Programs Host_Programs Host_Tests

dist:
	rm -rf $(DIST_TMPDIR)/$(DIST_ROOT) $(DIST_ROOT).tar.bz2
	mkdir -p $(DIST_TMPDIR)/$(DIST_ROOT)
	tar -c . | tar -x -C $(DIST_TMPDIR)/$(DIST_ROOT)
	tar -C $(DIST_TMPDIR) -c -j -f $(DIST_ROOT).tar.bz2 --exclude '*.[dom]*' \
	  $(patsubst %,$(DIST_ROOT)/%,$(DIST_LIST))
	rm -rf $(DIST_TMPDIR)/$(DIST_ROOT)
