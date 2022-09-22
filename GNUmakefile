# vim: set ft=make ts=8 sw=0 tw=0 noet list :
##############################################################################

# POSIX tools
COMMAND ?= command -p -v
GREP    ?= grep
PRINTF  ?= printf
RM      ?= rm -f
SHELL   := /bin/sh
TEST    := test
TRUE    := true

##############################################################################

# GNU time
XGTIME  ?= $(shell $(COMMAND) gtime || $(COMMAND) time)
ifneq ($(XGTIME),)
  GTIME ?= $(XGTIME) -f '%E'
else
  GTIME ?= time -p
endif

##############################################################################

# Empty string
BLANK  :=

##############################################################################

# Source files
SOURCE  = divmnu.c

##############################################################################

# Default V (verbose mode)
ifndef V
 V = 0
endif

# Verbose mode
ifneq ($(V),0)
  V       = 1 # Verbose enabled
  SETV    = set -x
else
  V       = 0 # Verbose disabled
  SETV    = set +x; $(PRINTF) '\r\t Make %s ...\n' "$@" 2> /dev/null |      \
            $(GREP) -v "Make clean \.\.\.$$"            2> /dev/null |      \
            $(GREP) -v "Make distclean \.\.\.$$"        2> /dev/null |      \
            $(GREP) -v "Make test \.\.\.$$"             2> /dev/null |      \
            $(GREP) -v "Make check \.\.\.$$"            2> /dev/null
endif

##############################################################################

# Default CC
CC ?= cc

# Notify about non-default CC
ifneq ($(V),1)
  ifneq ($(CC),cc)
    $(info $(BLANK)	 CC set to "$(CC)")
  endif
endif

##############################################################################

# Default CFLAGS
ifndef CFLAGS
  CFLAGS  = -Wall -O2
endif

# Notify about non-default CFLAGS
ifneq ($(V),1)
  ifneq ($(CFLAGS),-Wall -O2)
    $(info $(BLANK)	 CFLAGS set to "$(CFLAGS)")
  endif
endif

##############################################################################

# Default LDFLAGS
LDFLAGS  ?=

# Notify about non-default LDFLAGS
ifneq ($(V),1)
  ifneq ($(LDFLAGS),)
    $(info $(BLANK)	 LDFLAGS set to "$(LDFLAGS)")
  endif
endif

##############################################################################

# Output
OUT = divmnu-original                                                        \
      divmnu-sub_mul_borrow                                                  \
      divmnu-mul_rsub_carry                                                  \
      divmnu-sub_mul_borrow_2stage                                           \
      divmnu-mul_rsub_carry_2stage_0                                         \
      divmnu-mul_rsub_carry_2stage_1                                         \
      divmnu-mul_rsub_carry_2stage_2                                         \
      divmnu-madded_subfe

##############################################################################

# Default goal
.DEFAULT_GOAL := all

##############################################################################

# All goal
.PHONY: all
all:
	+@$(MAKE) -s build && $(MAKE) -s test

##############################################################################

# Build goal
.PHONY: build
build: $(OUT)

##############################################################################

# Clean goal
.PHONY: clean distclean
clean distclean:
ifneq ($(V),1)
	-@$(PRINTF) '\r\t %s\n' "Cleaning up ..." 2> /dev/null
endif
	@$(SETV); $(RM) $(OUT) core a.out *~ *.o *.ln *.s *.bak > /dev/null

##############################################################################

# Targets
divmnu-original: $(SOURCE)
	@$(SETV); $(CC) $< $(CFLAGS) $(LDFLAGS) -DORIGINAL                   \
	  -o $@

divmnu-sub_mul_borrow: $(SOURCE)
	@$(SETV); $(CC) $< $(CFLAGS) $(LDFLAGS) -DSUB_MUL_BORROW             \
	  -o $@

divmnu-mul_rsub_carry: $(SOURCE)
	@$(SETV); $(CC) $< $(CFLAGS) $(LDFLAGS) -DMUL_RSUB_CARRY             \
	  -o $@

divmnu-sub_mul_borrow_2stage: $(SOURCE)
	@$(SETV); $(CC) $< $(CFLAGS) $(LDFLAGS) -DSUB_MUL_BORROW_2_STAGE     \
	  -o $@

divmnu-mul_rsub_carry_2stage_0: $(SOURCE)
	@$(SETV); $(CC) $< $(CFLAGS) $(LDFLAGS) -DMUL_RSUB_CARRY_2_STAGE     \
	  -o $@

divmnu-mul_rsub_carry_2stage_1: $(SOURCE)
	@$(SETV); $(CC) $< $(CFLAGS) $(LDFLAGS) -DMUL_RSUB_CARRY_2_STAGE1    \
	  -o $@

divmnu-mul_rsub_carry_2stage_2: $(SOURCE)
	@$(SETV); $(CC) $< $(CFLAGS) $(LDFLAGS) -DMUL_RSUB_CARRY_2_STAGE2    \
	  -o $@

divmnu-madded_subfe: $(SOURCE)
	@$(SETV); $(CC) $< $(CFLAGS) $(LDFLAGS) -DMADDED_SUBFE               \
	  -o $@

##############################################################################

# Test goal
.PHONY: test check
test check: $(OUT)
	@failed=0;                                                           \
	 for test in $(OUT); do                                              \
	   $(TEST) $(V) -eq 1 > /dev/null 2>&1 &&                            \
	     $(PRINTF) '%s ./%s\n'                                           \
	       "$(GTIME)" "$${test:?}" 2> /dev/null;                         \
	   $(TEST) $(V) -ne 1 > /dev/null 2>&1 &&                            \
	     $(PRINTF) '\r\t Test %.34s '                                    \
	       "$${test:?} ................................ " 2> /dev/null;  \
	   $(GTIME) ./$${test:?} ||                                          \
	     $(PRINTF) '\n\r\t Failure %s ...\n\n'                           \
	       "$$(( failed=failed + 1 ))" 2> /dev/null;                     \
	 done;                                                               \
	 exit $${failed:?}
ifneq ($(V),1)
	-@$(PRINTF) '\r\t %s\n' "Test completed successfully!" 2> /dev/null
endif

##############################################################################
