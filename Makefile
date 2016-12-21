# -*- makefile-gmake -*-

PACKAGE_NAME = ctds

# Python version support
SUPPORTED_PYTHON_VERSIONS := \
    2.6 \
    2.7 \
    3.3 \
    3.4 \
    3.5

# FreeTDS versions to test against. This should
# be the latest of each minor release.
# Note: These version strings *must* match the versions
# on ftp://ftp.freetds.org/pub/freetds/stable/.
CHECKED_FREETDS_VERSIONS := \
    0.91.112 \
    0.92.405 \
    0.95.0 \
    0.95.95 \
    1.00.15

# FreeTDS version to test against.
ifndef FREETDS_VERSION
    FREETDS_VERSION := $(lastword $(CHECKED_FREETDS_VERSIONS))
endif

define CHECK_PYTHON
    $(if $(shell which python$(strip $(1))), $(strip $(1)))
endef

PYTHON_VERSIONS := $(strip $(foreach V, $(SUPPORTED_PYTHON_VERSIONS), $(call CHECK_PYTHON, $(V))))
DEFAULT_PYTHON_VERSION := $(lastword $(PYTHON_VERSIONS))

ifndef VIRTUALENV
    VIRTUALENV = python -m virtualenv
endif

VALGRIND_FLAGS := \
    --leak-check=summary \
    --fullpath-after=/ \
    --num-callers=24 \
    --malloc-fill=0xa0 \
    --free-fill=0xff \
    --log-file=valgrind.log

# Local directories
BUILD_DIR := build
HTML_BUILD_DIR := $(BUILD_DIR)/docs/html

CTDS_VERSION := $(strip $(shell python setup.py --version))

# Platform-specific values.
ifeq "$(shell uname -s)" "Darwin"
    FREETDS_MAC_OSX := 1
endif

ifdef FREETDS_MAC_OSX
    LD_LIBRARY_PATH := DYLD_FALLBACK_LIBRARY_PATH
else
    LD_LIBRARY_PATH := LD_LIBRARY_PATH
endif


# Functions to generate the FreeTDS paths.
#
# $(eval $(call FREETDS_(ROOT|INCLUDE|LIB), <freetds_version>))
#
FREETDS_ROOT = $(abspath $(BUILD_DIR)/freetds/$(strip $(1)))
FREETDS_INCLUDE = $(call FREETDS_ROOT, $(1))/include
FREETDS_LIB = $(call FREETDS_ROOT, $(1))/lib


# Function to generate freetds-<version> rules to build local
# copies of FreeTDS.
#
# $(eval $(call FREETDS_RULE, <freetds_version>))
#
define FREETDS_RULE
# Download and extract the FreeTDS source code.
$(BUILD_DIR)/freetds-$(strip $(1)):
	mkdir -p $(BUILD_DIR)
	curl -o "$(BUILD_DIR)/freetds-$(strip $(1)).tar.gz" \
        'ftp://ftp.freetds.org/pub/freetds/stable/freetds-$(strip $(1)).tar.gz'
	tar -xzf $(BUILD_DIR)/freetds-$(strip $(1)).tar.gz -C $(BUILD_DIR)

# The version-specific build sentinel.
FREETDS_$(strip $(1))_BUILD := $(BUILD_DIR)/freetds-$(strip $(1)).build

# Configure, make, make install from FreeTDS source.
$$(FREETDS_$(strip $(1))_BUILD): $(BUILD_DIR)/freetds-$(strip $(1))
	cd $$< && \
		CFLAGS="$$$$CFLAGS -g" make distclean; \
        ./configure \
            --prefix="$$(call FREETDS_ROOT, $(strip $(1)))" \
            --disable-odbc --disable-apps --disable-server --disable-pool \
		    && \
        make && \
        make install
	touch $$@

.PHONY: freetds-$(strip $(1))
freetds-$(strip $(1)): $$(FREETDS_$(strip $(1))_BUILD)
endef


# Function to generate virtualenv rules
#
# $(eval $(call ENV_RULE, <env_name>, <python_version>, <freetds_version>, <packages>, <commands>))
#
define ENV_RULE
ENV_$(strip $(1))_BUILD := $(BUILD_DIR)/$(strip $(1)).build

$$(ENV_$(strip $(1))_BUILD): freetds-$(strip $(3))
	$$(VIRTUALENV) -p python$(strip $(2)) $(BUILD_DIR)/$(strip $(1))
	$(call ENV_PIP, $(1)) install --upgrade pip
	$(if $(strip $(4)),$(call ENV_PIP, $(1)) install $(strip $(4)))
	touch $$@

.PHONY: $(strip $(1))
$(strip $(1)): $$(ENV_$(strip $(1))_BUILD)
$(call $(5), $(1), $(3))
endef


# Function to generate check rules
#
# $(eval $(call CHECK_RULE, <freetds_version>))
#
define CHECK_RULE
.PHONY: check-$(strip $(1))
check-$(strip $(1)): test_config $(foreach PV, $(PYTHON_VERSIONS), test_$(PV)_$(strip $(1)))
endef


# Function to generate an environment rule's python interpreter.
#   Usage: ENV_PYTHON(env_name)
ENV_PYTHON = $(abspath $(BUILD_DIR)/$(strip $(1))/bin/python) -E

# Function to generate an environment rule's python interpreter.
#   Usage: ENV_PYTHON(env_name)
ENV_COVERAGE = $(abspath $(BUILD_DIR)/$(strip $(1))/bin/coverage)

# Function to generate an environment rule's pip utility.
#   Usage: ENV_PIP(env_name, freetds_version)
# Note the bin/pip script is not launched directly to avoid the following
# issue with #! length limits: https://github.com/pypa/virtualenv/issues/596.
ENV_PIP = $(ENV_PYTHON) $(BUILD_DIR)/$(strip $(1))/bin/pip

define TEST_COMMANDS
	# Always rebuild with debugging symbols and coverage enabled.
	TDS_COVER=1 DEBUG=1 $(call ENV_PIP, $(1)) install -v -e . -e .[tests] \
        --global-option=build_ext \
        --global-option="-t$(BUILD_DIR)/$(strip $(1))" \
        --global-option=build_ext \
        --global-option="-I$(call FREETDS_INCLUDE, $(strip $(2)))" \
        --global-option=build_ext \
        --global-option="-L$(call FREETDS_LIB, $(strip $(2)))" \
        --global-option=build_ext --global-option="-R$(call FREETDS_LIB, $(strip $(2)))" \
        --global-option=build_ext --global-option="-f"

	$(LD_LIBRARY_PATH)=$(call FREETDS_LIB, $(strip $(2))) \
        $(if $(VALGRIND), valgrind $(VALGRIND_FLAGS)) \
        $(call ENV_COVERAGE, $(1)) run --branch --source '$(PACKAGE_NAME)' \
            setup.py test $(if $(TEST),-s $(TEST))
endef

define PYLINT_COMMANDS
	$(ENV_PYTHON) $(BUILD_DIR)/$(strip $(1))/bin/pylint $(PACKAGE_NAME)
endef

define PYTHON_COVERAGE_COMMANDS
	$(LD_LIBRARY_PATH)=$(call FREETDS_LIB, $(strip $(2))) \
        $(call ENV_COVERAGE, $(1)) html -d $(abspath $(BUILD_DIR)/coverage)
	$(LD_LIBRARY_PATH)=$(call FREETDS_LIB, $(strip $(2))) \
        $(call ENV_COVERAGE, $(1)) report --fail-under 100
endef

define DOC_COMMANDS
	$(call ENV_PIP, $(1)) install --force-reinstall -v -e .
	$(ENV_PYTHON) -m sphinx -n -a -E -d $(BUILD_DIR)/$(strip $(1)) docs $(HTML_BUILD_DIR)
endef


# Help
.PHONY: help
help:
	@echo "usage: make [check|clean|cover|pylint|setup|test]"
	@echo
	@echo "    check"
	@echo "        Run tests against all installed versions of Python and"
	@echo "        the following versions of FreeTDS: $(CHECKED_FREETDS_VERSIONS)."
	@echo
	@echo "    clean"
	@echo "        Clean source tree."
	@echo
	@echo "    cover"
	@echo "        Generate code coverage for c source code."
	@echo
	@echo "    doc"
	@echo "        Generate documentation."
	@echo
	@echo "    publish"
	@echo "        Publish egg and docs to pypi"
	@echo
	@echo "    pylint"
	@echo "        Run pylint over all *.py files."
	@echo
	@echo "    setup"
	@echo "        Install required packages (Debian systems only)."
	@echo
	@echo "    test"
	@echo "        Run tests using the default Python version ($(DEFAULT_PYTHON_VERSION)) and"
	@echo "        the default version of FreeTDS ($(FREETDS_VERSION))."
	@echo
	@echo "    Optional variables:"
	@echo "      TEST - Optional test specifier. e.g. \`make test TEST=ctds.tests.test_tds_connect\`"
	@echo "      VALGRIND - Run tests under \`valgrind\`. e.g. \`make test VALGRIND=1\`"
	@echo "      FREETDS_VERSION - Specify the version of FreeTDS to build and use. Defaults to $(FREETDS_VERSION)."


# check
.PHONY: check
check: test_config $(foreach PV, $(PYTHON_VERSIONS), $(foreach FV, $(CHECKED_FREETDS_VERSIONS), test_$(PV)_$(FV)))

# check-*
$(foreach FREETDS_VERSION, $(CHECKED_FREETDS_VERSIONS), $(eval $(call CHECK_RULE, $(FREETDS_VERSION))))

# clean
.PHONY: clean
clean:
	git clean -dfX

.PHONY: cover
cover: test python-cover
	gcov -o $(BUILD_DIR)/test_$(DEFAULT_PYTHON_VERSION)_$(FREETDS_VERSION)/ctds ctds/*.c
	mkdir -p $(abspath $(BUILD_DIR)/cover)
	gcovr --delete --root=$(abspath .) --html --html-details --output=$(abspath $(BUILD_DIR)/cover/index.html)

# python-cover
$(eval $(call ENV_RULE, python-cover, $(DEFAULT_PYTHON_VERSION), $(FREETDS_VERSION), coverage, PYTHON_COVERAGE_COMMANDS))

# doc
$(eval $(call ENV_RULE, doc, $(DEFAULT_PYTHON_VERSION), $(FREETDS_VERSION), sphinx sphinx_rtd_theme, DOC_COMMANDS))

publish: doc
	git tag -a v$(CTDS_VERSION) -m "v$(CTDS_VERSION)"
	git push --tags
	python setup.py sdist upload
	python setup.py upload_docs --upload-dir=$(HTML_BUILD_DIR)

# pylint
$(eval $(call ENV_RULE, pylint, $(DEFAULT_PYTHON_VERSION), $(FREETDS_VERSION), pylint, PYLINT_COMMANDS))

# setup (debian-only)
.PHONY: setup
setup:
	sudo add-apt-repository -y ppa:fkrull/deadsnakes
	sudo apt-get update
	sudo apt-get install -y \
		python-pip \
		python-virtualenv \
		$(foreach P, $(PYTHON_VERSIONS),$(if $(shell which python$P),,python$P python$P-dev))
	sudo pip install -U pip virtualenv

# test
.PHONY: test
test: test_config test_$(DEFAULT_PYTHON_VERSION)_$(FREETDS_VERSION)

test_config: ctds/tests/database.ini

ctds/tests/database.ini:
	@cp ctds/tests/database.ini.in $@
	@echo "Test database configuration required in $@"
	@/bin/false

# freetds-*
$(foreach FV, $(CHECKED_FREETDS_VERSIONS), $(eval $(call FREETDS_RULE, $(FV))))

# test_*
$(foreach PV, $(PYTHON_VERSIONS), $(foreach FV, $(CHECKED_FREETDS_VERSIONS), $(eval $(call ENV_RULE, test_$(PV)_$(FV), $(PV), $(FV), coverage, TEST_COMMANDS))))
