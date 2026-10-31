PRODUCT = touchtyping-keyboard.pdx
.DEFAULT_GOAL := all
SDK ?= $(if $(PLAYDATE_SDK_PATH),$(PLAYDATE_SDK_PATH),$(HOME)/Developer/PlaydateSDK)
PYTHON ?= python3

# Host tests do not load SDK build rules or require an ARM toolchain.
.PHONY: test test-sanitize test-core check-stack
test:
	$(PYTHON) tools/run_tests.py --sdk "$(SDK)"
test-sanitize:
	$(PYTHON) tools/run_tests.py --sdk "$(SDK)" --sanitize
test-core:
	$(PYTHON) tools/run_tests.py --core-only
check-stack: device_bin
	$(PYTHON) tools/check_stack.py build --limit 2048

# The SDK creates internal debug packages; these are not release artifacts.
# Set SDK (or PLAYDATE_SDK_PATH) to the locally installed Playdate SDK.
ifneq ($(filter-out test test-sanitize test-core,$(or $(MAKECMDGOALS),all)),)
VPATH += src
SRC = examples/keyboard_minimal.c src/keyboard.c src/keyboard_model.c src/emoji_set.c src/unicode.c src/unicode_font.c src/input_field.c src/pill.c
UINCDIR = src
UASRC =
UDEFS =
UADEFS =
ULIBDIR =
ULIBS = -lm
include $(SDK)/C_API/buildsupport/common.mk
# The Playdate C stack is 10 KB. Bound individual application frames and retain
# reports for review of nested calls; this is not a whole-call-chain proof.
CPFLAGS += -Wstack-usage=2048 -Werror=stack-usage=2048 -fstack-usage
endif
