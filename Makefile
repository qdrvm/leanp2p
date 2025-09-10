SHELL := /bin/bash
PROJECT := $(shell pwd)
CI_DIR := $(PROJECT)/.ci

VENV ?= $(PROJECT)/.venv
BUILD ?= $(PROJECT)/.build
VCPKG ?= $(PROJECT)/.vcpkg
PATH = $(VENV)/bin:$(shell echo $$PATH)

ifneq (,$(wildcard $(CI_DIR)/.env))
    include $(CI_DIR)/.env
    export
endif

OS_TYPE := $(shell bash -c 'source $(CI_DIR)/scripts/detect_os.sh && detect_os')


all: init_all configure build test

init_all: init init_py init_vcpkg

os:
	@echo "=== Detected OS: $(OS_TYPE)"

init:
	@echo "=== Initializing..."
	$(CI_DIR)/scripts/init.sh

init_py:
	@echo "=== Initializing Python..."
	source $(CI_DIR)/scripts/init_py.sh && init_py

init_vcpkg:
	@echo "=== Initializing Vcpkg..."
	source $(CI_DIR)/scripts/init_vcpkg.sh && init_vcpkg

configure:
	@echo "=== Configuring..."
	export PATH="$$HOME/.cargo/bin:$$PATH" && \
	source $$HOME/.cargo/env 2>/dev/null || true && \
	VCPKG_ROOT=$(VCPKG) cmake --preset=default -DPython3_EXECUTABLE="$(VENV)/bin/python3" -B $(BUILD) $(PROJECT)

build:
	@echo "=== Building..."
	cmake --build $(BUILD) 

test:
	@echo "=== Testing..."
	ctest --test-dir $(BUILD)

clean_all:
	@echo "=== Cleaning..."
	rm -rf $(VENV) $(BUILD) $(VCPKG)	

.PHONY: all init_all os init init_py init_vcpkg configure build test clean_all
