# Makefile wrapper to allow old make commands to use waf build system

MAKE_BOARDS = vrbrain vrbrain-v40 vrbrain-v45 vrbrainv-50 vrbrain-v51 vrbrain-v52 vrubrain-v51 vrubrain-v52 vrhero-v10 qflight 

ifneq ($(strip $(foreach board,$(MAKE_BOARDS), $(findstring $(board), $(MAKECMDGOALS)))),)
include Makefile.old
else

num_targets = $(words $(MAKECMDGOALS))
ifneq ($(num_targets), 0)
ifneq ($(num_targets), 1)
$(error Only one target is allowed)
endif
endif

SOURCEROOT = $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/..)

SRCROOT	:=	$(realpath $(dir $(firstword $(MAKEFILE_LIST))))
BUILDDIR :=	$(lastword $(subst /, ,$(SRCROOT)))

# Workaround a $(lastword ) bug on cygwin
ifeq ($(BUILDDIR),)
  WORDLIST		:=	$(subst /, ,$(SRCROOT))
  BUILDDIR		:=	$(word $(words $(WORDLIST)),$(WORDLIST))
endif

define target_template
ifneq ($(2),)
$(1) : TARGET_BOARD=$(2)
ifneq ($(3),)
$(1) : FRAME=-$(3)
endif
ifneq ($(4),)
$(1) : DEBUG_FLAG=--debug
$(1) : BIN_DIR=$(2)-debug
endif
ifneq ($(5),)
$(1) : UPLOAD_FLAG=--upload
endif
ifneq ($(6),)
$(1) : BINARY_EXTENSION=".px4"
$(1) : BINARY_EXTENSION_DEST=".px4"
endif
$(1) : all
endif
endef

TARGET_BOARD=sitl
TARGET_BUILD=$(BUILDDIR)
BIN_DIR=$(TARGET_BOARD)
DEBUG_FLAG=
UPLOAD_FLAG=
FRAME=
BINARY_EXTENSION=
BINARY_EXTENSION_DEST=".elf"

ifeq ($(BUILDDIR),ArduPlane)
TARGET_BUILD=plane
BINARY_NAME=arduplane
endif

ifeq ($(BUILDDIR),ArduCopter)
TARGET_BUILD=copter
BINARY_NAME=arducopter
FRAME=-quad
endif

ifeq ($(BUILDDIR),APMrover2)
TARGET_BUILD=rover
BINARY_NAME=ardurover
endif

ifeq ($(BUILDDIR),AntennaTracker)
TARGET_BUILD=antennatracker
BINARY_NAME=antennatracker
endif

FRAMES = quad tri hexa y6 octa octa-quad heli single coax
BOARDS = px4-v1 px4-v2 px4-v4 sitl linux erleboard pxf navio navio2 raspilot bbbmini minlure erlebrain2 bhat pxfmini bebop disco

cmd_board = $(strip $(foreach board, $(BOARDS), $(findstring $(board), $(MAKECMDGOALS))))
cmd_frame = $(strip $(foreach frame, $(FRAMES), $(findstring $(frame), $(MAKECMDGOALS))))
cmd_debug = $(findstring debug, $(MAKECMDGOALS))
cmd_upload = $(findstring upload, $(MAKECMDGOALS))
cmd_extension = $(findstring px4, $(MAKECMDGOALS))

$(eval $(call target_template,$(MAKECMDGOALS),$(cmd_board),$(cmd_frame),$(cmd_debug),$(cmd_upload),$(cmd_extension)))

all:
	@echo SOURCEROOT=$(SOURCEROOT)
	@echo BUILDDIR=$(BUILDDIR)
	@echo TARGET_BOARD=$(TARGET_BOARD)
	@echo TARGET_BUILD=$(TARGET_BUILD)
	@echo FRAME=$(FRAME:-%=%)
	@echo DEBUG_FLAG=$(DEBUG_FLAG)
	@echo UPLOAD_FLAG=$(UPLOAD_FLAG)
	@echo BIN_DIR=$(BIN_DIR)
	@echo BINARY_NAME=$(BINARY_NAME)$(FRAME)
	@echo BINARY_EXTENSION=$(BINARY_EXTENSION)
	@echo BINARY_EXTENSION_DEST=$(BINARY_EXTENSION_DEST)
	cd $(SOURCEROOT) && modules/waf/waf-light --board $(TARGET_BOARD) $(DEBUG_FLAG) configure
	cd $(SOURCEROOT) && modules/waf/waf-light --targets bin/$(BINARY_NAME)$(FRAME) $(UPLOAD_FLAG)
	@cp $(SOURCEROOT)/build/$(BIN_DIR)/bin/$(BINARY_NAME)$(FRAME)$(BINARY_EXTENSION) $(BUILDDIR)$(BINARY_EXTENSION_DEST)
	@ls -l $(BUILDDIR)$(BINARY_EXTENSION_DEST)
	@echo "Build done $(BUILDDIR)$(BINARY_EXTENSION_DEST)"

clean:
	@cd $(SOURCEROOT) && modules/waf/waf-light distclean

cleandep: clean

px4-clean: clean

px4-cleandep: px4-clean cleandep

.NOTPARALLEL:

endif
