# Convenience targets. Requires clang, cmake, ninja; the web target needs emsdk
# (EMSDK env var, or ~/emsdk).
SHELL := /bin/bash
ROOT := $(abspath .)
EMSDK ?= $(HOME)/emsdk
LINUX_TOOLCHAIN := $(ROOT)/evogine/src/platform/linux/toolchain/clang-toolchain.cmake

.PHONY: all evogen linux web run test pages clean

all: linux

evogen:
	cmake -S evogine/tools/evogen -G Ninja -B build-evogen --toolchain=$(LINUX_TOOLCHAIN) \
		-DCMAKE_BUILD_TYPE=Release -DIMGUI_ENABLED=ON -DIMGUI_EXTENSION_ENABLED=ON \
		"-DCMAKE_CXX_FLAGS=-I$(ROOT)/evogine/thirdparty/sdl/include"
	ninja -C build-evogen

linux:
	cmake -S . -G Ninja -B build-linux --toolchain=$(LINUX_TOOLCHAIN) -DCMAKE_BUILD_TYPE=Release
	ninja -C build-linux

web:
	source $(EMSDK)/emsdk_env.sh >/dev/null 2>&1 && \
	cmake -S . -G Ninja -B build-web --toolchain=$(ROOT)/cmake/emscripten-toolchain.cmake -DCMAKE_BUILD_TYPE=Release && \
	ninja -C build-web

run: linux
	./build-linux/Release/evo-tournament

test: linux
	./build-linux/Release/evo-tournament -selftest
	./build-linux/Release/evo-tournament -nettest

# Copies the web build into ./site (index.html + assets) for GitHub Pages.
pages: web
	rm -rf site && mkdir -p site
	cp build-web/evo-tournament.html site/index.html
	cp build-web/evo-tournament.js build-web/evo-tournament.wasm build-web/evo-tournament.data site/
	touch site/.nojekyll

clean:
	rm -rf build-linux build-web site
