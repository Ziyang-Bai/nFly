NSPIRE_CC ?= nspire-gcc
GENZEHN ?= genzehn
MAKE_PRG ?= make-prg
PYTHON ?= python3
HOST_CC ?= cc
SOURCE ?= .cache/connectome.bin.gz

CPPFLAGS := -Isrc
CFLAGS := -std=c99 -O2 -marm -Wall -Wextra -Wshadow -Werror -ffunction-sections -fdata-sections -MMD -MP
LDFLAGS := -Wl,--gc-sections
SOURCES := src/main.c src/render.c src/brain.c src/world.c src/groups.c
OBJECTS := $(SOURCES:src/%.c=build/%.o)

.PHONY: all app data release help clean check
all: app data dist/LICENSE
app: dist/nFly.tns
data: dist/connectome.tns dist/connectome.json
release: dist/nFly.zip

help:
	@echo "nFly: FlyBrain for Ndless on TI-Nspire CX / CX II (64 MB RAM)."
	@echo "make check       Run host tests (C99 compiler and zlib headers)."
	@echo "make app         Build dist/nFly.tns with the Ndless SDK."
	@echo "make data        Generate connectome.tns and its JSON metadata."
	@echo "make all release Build the application, data and dist/nFly.zip."
	@echo "Offline input:   make data SOURCE=/path/to/connectome.bin.gz"
	@echo "Install connectome.tns and nFly.tns in the same calculator folder."
	@echo "See README.md for controls, data preparation and build requirements."

build dist:
	mkdir -p $@

build/%.o: src/%.c | build
	$(NSPIRE_CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/nFly.elf: $(OBJECTS)
	$(NSPIRE_CC) $(OBJECTS) $(LDFLAGS) -lz -lm -o $@

build/nFly.zehn: build/nFly.elf
	$(GENZEHN) --input $< --output $@ --name nFly --compress --ndless-min 36 --32MB-support 0 --uses-lcd-blit true

dist/nFly.tns: build/nFly.zehn | dist
	$(MAKE_PRG) $< $@

dist/connectome.tns dist/connectome.json &: tools/pack_connectome.py | dist
	$(PYTHON) $< --source "$(SOURCE)" --output dist/connectome.tns

dist/LICENSE: LICENSE | dist
	cp $< $@

dist/nFly.zip: dist/nFly.tns dist/connectome.tns dist/connectome.json dist/LICENSE
	$(PYTHON) -m zipfile -c $@ $^

build/test_brain: tests/test_brain.c src/brain.c src/brain.h src/groups.h | build
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Werror -Isrc tests/test_brain.c src/brain.c -lz -lm -o $@

build/test_world: tests/test_world.c src/world.c src/brain.c src/world.h src/brain.h src/groups.h | build
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Werror -Isrc tests/test_world.c src/world.c src/brain.c -lz -lm -o $@

build/test_render: tests/test_render.c src/render.c src/world.c src/brain.c src/groups.c src/render.h src/world.h src/brain.h src/groups.h | build
	$(HOST_CC) -std=c99 -O2 -Wall -Wextra -Werror -Isrc tests/test_render.c src/render.c src/world.c src/brain.c src/groups.c -lz -lm -o $@

check: build/test_brain build/test_world build/test_render
	./build/test_brain
	./build/test_world
	./build/test_render

clean:
	rm -rf build dist

-include $(OBJECTS:.o=.d)
