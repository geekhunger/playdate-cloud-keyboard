SDK ?= $(PLAYDATE_SDK_PATH)
PDC ?= $(if $(SDK),$(SDK)/bin/pdc,pdc)
.PHONY: build test artwork
build:
	mkdir -p build
	"$(PDC)" Source build/CloudKeyboard.pdx
test:
	lua tests/test_keyboard.lua
artwork:
	python3 tools/build_artwork.py
