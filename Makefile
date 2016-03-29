# -*- makefile-gmake -*-

PACKAGE_NAME = ctds

# FreeTDS version to test against.
ifndef FREETDS_VERSION
    FREETDS_VERSION := 0.95.87
endif

# Python version support
SUPPORTED_PYTHON_VERSIONS := \
    2.6 \
    2.7 \
    3.3 \
    3.4 \
    3.5

define CHECK_PYTHON
    $(if $(shell which python$(strip $(1))), $(strip $(1)))
endef

PYTHON_VERSIONS := $(strip $(foreach V, $(SUPPORTED_PYTHON_VERSIONS), $(call CHECK_PYTHON, $(V))))
DEFAULT_PYTHON_VERSION := $(lastword $(PYTHON_VERSIONS))

ifndef VIRTUALENV
    VIRTUALENV = virtualenv
endif

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

# FreeTDS build rule.
# This will generate a target to build the specific version of FreeTDS as specified in
# FREETDS_VERSION.
$(BUILD_DIR)/freetds-$(FREETDS_VERSION):
	mkdir -p $(BUILD_DIR)
	curl -o "$(BUILD_DIR)/freetds-$(FREETDS_VERSION).tar.gz" \
        'ftp://ftp.freetds.org/pub/freetds/stable/freetds-$(FREETDS_VERSION).tar.gz'
	tar -xzf $(BUILD_DIR)/freetds-$(FREETDS_VERSION).tar.gz -C $(BUILD_DIR)

# The version-specific build sentinel.
FREETDS_$(FREETDS_VERSION)_BUILD := $(BUILD_DIR)/freetds-$(FREETDS_VERSION).build

$(FREETDS_$(FREETDS_VERSION)_BUILD): $(BUILD_DIR)/freetds-$(FREETDS_VERSION)
	cd $< && \
		CFLAGS="$$CFLAGS -g" make distclean; \
        ./configure \
            --prefix="$(call FREETDS_ROOT, $(FREETDS_VERSION))" \
            --disable-odbc --disable-apps --disable-server --disable-pool \
		    && \
        make && \
        make install
	touch $@

freetds-$(FREETDS_VERSION): $(FREETDS_$(FREETDS_VERSION)_BUILD)

# Function to generate virtualenv rules
#
# $(eval $(call ENV_RULE, <env_name>, <python_version>, <packages>, <commands>))
#
define ENV_RULE
BUILD_$(strip $(1)) := $(BUILD_DIR)/$(strip $(1)).build

$$(BUILD_$(strip $(1)))_$(FREETDS_VERSION): freetds-$(FREETDS_VERSION)
	$$(VIRTUALENV) -p python$(strip $(2)) $(BUILD_DIR)/$(strip $(1))
	$(call ENV_PIP, $(1)) install --upgrade pip
	$(if $(strip $(3)),$(call ENV_PIP, $(1)) install $(strip $(3)))
	touch $$@

.PHONY: $(strip $(1))
$(strip $(1)): $$(BUILD_$(strip $(1)))_$(FREETDS_VERSION)
$(call $(4), $(1))
endef

# Function to generate an environment rule's python interpreter.
#   Usage: ENV_PYTHON(env_name)
ENV_PYTHON = $(BUILD_DIR)/$(strip $(1))/bin/python -E

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
        --global-option="-I$(call FREETDS_INCLUDE, $(FREETDS_VERSION))" \
        --global-option=build_ext \
        --global-option="-L$(call FREETDS_LIB, $(FREETDS_VERSION))" \
        --global-option=build_ext --global-option="-R$(call FREETDS_LIB, $(FREETDS_VERSION))" \
        --global-option=build_ext --global-option="-f"

	$(LD_LIBRARY_PATH)=$(call FREETDS_LIB, $(FREETDS_VERSION)) \
        $(call ENV_PYTHON, $(1)) setup.py test $(if $(TEST),-s $(TEST))
endef

define PYLINT_COMMANDS
	$(ENV_PYTHON) $(BUILD_DIR)/$(strip $(1))/bin/pylint $(PACKAGE_NAME)
endef

define DOC_COMMANDS
	$(call ENV_PIP, $(1)) install -v -e .
	$(ENV_PYTHON) -m sphinx -n -a -d $(BUILD_DIR)/$(strip $(1)) docs $(HTML_BUILD_DIR)
endef


# Help
.PHONY: help
help:
	@echo "usage: make [check|clean|cover|pylint|setup|test]"
	@echo
	@echo "    check"
	@echo "        Run tests against all versions of Python."
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
	@echo "        the default version of FreeTDS ($(DEFAULT_FREETDS_VERSION))."
	@echo
	@echo "    Optional variables:"
	@echo "      TEST - Optional test specifier. e.g. \`make test TEST=ctds.tests.test_tds_connect\`"
	@echo "      FREETDS_VERSION - Specify the version of FreeTDS to build and use. Defaults to $(FREETDS_VERSION)."


# check
.PHONY: check
check: test_config $(foreach V, $(PYTHON_VERSIONS), test_$(V)_$(FREETDS_VERSION))

# clean
.PHONY: clean
clean:
	git clean -dfX

.PHONY: cover
cover: test
	gcov -o $(BUILD_DIR)/test_$(DEFAULT_PYTHON_VERSION)_$(FREETDS_VERSION)/ctds ctds/*.c

# doc
$(eval $(call ENV_RULE, doc, $(DEFAULT_PYTHON_VERSION), sphinx sphinx_rtd_theme, DOC_COMMANDS))

publish: doc
	git tag -a v$(CTDS_VERSION) -m "v$(CTDS_VERSION)"
	git push --tags
	python setup.py sdist upload
	python setup.py upload_docs --upload-dir=$(HTML_BUILD_DIR)

# pylint
$(eval $(call ENV_RULE, pylint, $(DEFAULT_PYTHON_VERSION), pylint, PYLINT_COMMANDS))

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
	cp ctds/tests/database.ini.in $@
	@echo "Test database configuration required in $@" && /bin/false

# test_*
$(foreach V, $(PYTHON_VERSIONS), $(eval $(call ENV_RULE, test_$(V)_$(FREETDS_VERSION), $(V), , TEST_COMMANDS)))
