# -*- makefile-gmake -*-

BUILDDIR ?= build

# Python version support
SUPPORTED_PYTHON_VERSIONS := \
    2.7 \
    3.3 \
    3.4 \
    3.5 \
    3.6 \
    3.7 \
    3.8 \
    3.9

CHECKED_PYTHON_VERSIONS := \
    2.7 \
    3.6 \
    3.7 \
    3.8 \
    3.9

VALGRIND_PYTHON_VERSIONS := \
    2.7.14 \
    3.6.3

# FreeTDS versions to test against. This should
# be the latest of each minor release.
# Note: These version strings *must* match the versions
# on http://www.freetds.org/files/stable/.
CHECKED_FREETDS_VERSIONS := \
    0.91.112 \
    0.92.405 \
    0.95.95 \
    1.00.80 \
    1.1.24 \
    1.2.10

# Valgrind FreeTDS versions are limited to one without sp_executesql support
# and one with.
VALGRIND_FREETDS_VERSIONS := \
    $(firstword $(CHECKED_FREETDS_VERSIONS)) \
    $(lastword $(CHECKED_FREETDS_VERSIONS))


DEFAULT_FREETDS_VERSION := $(lastword $(CHECKED_FREETDS_VERSIONS))

# Help
.PHONY: help
help:
	@echo "usage: make [check|clean|docs|pylint|test-$$FREETDS_VERSION|valgrind]"
	@echo
	@echo "    check"
	@echo "        Run tests against all supported versions of Python and"
	@echo "        the following versions of FreeTDS: $(CHECKED_FREETDS_VERSIONS)."
	@echo
	@echo "    clean"
	@echo "        Clean source tree, stop and remove test MS SQL Server container(s)."
	@echo
	@echo "    docs"
	@echo "        Generate documentation."
	@echo
	@echo "    pylint"
	@echo "        Run pylint over all *.py files."
	@echo
	@echo "    test-$$FREETDS_VERSION"
	@echo "        Run tests against FreeTDS version $$FREETDS_VERSION"
	@echo
	@echo "    valgrind"
	@echo "        Run tests using a debug build of Python versions ($(VALGRIND_PYTHON_VERSIONS)) and"
	@echo "        FreeTDS versions ($(VALGRIND_FREETDS_VERSIONS)) under valgrind."
	@echo
	@echo "    Optional variables:"
	@echo "      VERBOSE - Include more verbose output."

DARWIN := $(findstring Darwin,$(shell uname))

SQL_SERVER_DOCKER_IMAGE_NAME := ctds-unittest-sqlserver
VALGRIND_DOCKER_IMAGE_NAME = ctds-valgrind-python$(strip $(1))-$(strip $(2))


.PHONY: clean
clean: stop-sqlserver
	git clean -dfX
	docker images -q ctds-unittest-* | xargs $(if $(DARWIN),,-r) docker rmi

.PHONY: start-sqlserver
start-sqlserver:
	scripts/ensure-sqlserver.sh $(SQL_SERVER_DOCKER_IMAGE_NAME)

.PHONY: stop-sqlserver
stop-sqlserver:
	scripts/remove-sqlserver.sh $(SQL_SERVER_DOCKER_IMAGE_NAME)


# Function to generate rules for:
#   * building a docker image with a specific valgrind-Python/FreeTDS version
#   * running unit tests for a specific Python/FreeTDS version under valgrind
#
# Note: Python version *must* be exact, as it is built from source.
#
# $(eval $(call GENERATE_VALGRIND_RULES, <python_version>, <freetds_version>))
#
define GENERATE_VALGRIND_RULES
.PHONY: docker_valgrind_$(strip $(1))_$(strip $(2))
docker_valgrind_$(strip $(1))_$(strip $(2)):
	docker build $(if $(VERBOSE),,-q) \
        --build-arg "PYTHON_VERSION=$(strip $(1))" \
        --build-arg "FREETDS_VERSION=$(strip $(2))" \
        -f Dockerfile-valgrind \
        -t $(call VALGRIND_DOCKER_IMAGE_NAME, $(1), $(2)) \
        .

.PHONY: valgrind_$(strip $(1))_$(strip $(2))
valgrind_$(strip $(1))_$(strip $(2)): docker_valgrind_$(strip $(1))_$(strip $(2)) start-sqlserver
	docker run \
            --init --rm \
            -e DEBUG=1 \
            $(if $(TDSDUMP),-e TDSDUMP=$(TDSDUMP)) \
            $(if $(TEST),-e TEST=$(TEST)) \
            $(if $(VERBOSE),-e VERBOSE=$(VERBOSE)) \
            --network container:$(SQL_SERVER_DOCKER_IMAGE_NAME) \
        $(call VALGRIND_DOCKER_IMAGE_NAME, $(1), $(2)) \
        ./scripts/ctds-valgrind.sh
endef

$(foreach PV, $(VALGRIND_PYTHON_VERSIONS), $(foreach FV, $(VALGRIND_FREETDS_VERSIONS), $(eval $(call GENERATE_VALGRIND_RULES, $(PV), $(FV)))))

define VALGRIND_RULE
.PHONY: valgrind_$(strip $(1))
valgrind_$(strip $(1)): $(foreach FV, $(VALGRIND_FREETDS_VERSIONS), valgrind_$(strip $(1))_$(FV))
endef

$(foreach PV, $(VALGRIND_PYTHON_VERSIONS), $(eval $(call VALGRIND_RULE, $(PV))))

.PHONY: valgrind
valgrind: $(foreach PV, $(VALGRIND_PYTHON_VERSIONS), valgrind_$(PV))

# Function to generate rules for:
#   * downloading FreeTDS source
#   * compiling FreeTDS source
#
# $(eval $(call FREETDS_BUILD_RULE, <freetds_version>))
#
define FREETDS_BUILD_RULE
$(BUILDDIR)/src/freetds-$(strip $(1)).tar.gz:
	mkdir -p $(BUILDDIR)/src
	if [ `which wget` ]; then \
        wget 'https://www.freetds.org/files/stable/freetds-$(strip $(1)).tar.gz' -O $$(@); \
    else \
        curl -sSf 'https://www.freetds.org/files/stable/freetds-$(strip $(1)).tar.gz' -o $$(@); \
    fi

$(BUILDDIR)/src/freetds-$(strip $(1)): $(BUILDDIR)/src/freetds-$(strip $(1)).tar.gz
	tar -xzC $(BUILDDIR)/src -f $$(^)

$(BUILDDIR)/freetds-$(strip $(1))/include/sybdb.h: $(BUILDDIR)/src/freetds-$(strip $(1))
	mkdir -p $(BUILDDIR)/src/freetds-$(strip $(1))
	cd $(BUILDDIR)/src/freetds-$(strip $(1)) && \
    ./configure --prefix "$(abspath $(BUILDDIR)/freetds-$(strip $(1)))" && \
    make && \
    make install

.PHONY: freetds-$(strip $(1))
freetds-$(strip $(1)): $(BUILDDIR)/freetds-$(strip $(1))/include/sybdb.h

.PHONY: test-$(strip $(1))
test-$(strip $(1)): freetds-$(strip $(1))
	CTDS_INCLUDE_DIRS="$(abspath $(BUILDDIR)/freetds-$(strip $(1)))/include" \
    CTDS_LIBRARY_DIRS="$(abspath $(BUILDDIR)/freetds-$(strip $(1)))/lib" \
    CTDS_RUNTIME_LIBRARY_DIRS="$(abspath $(BUILDDIR)/freetds-$(strip $(1)))/lib" \
    tox -vv -e $$$$(tox -l | grep 'py[[:digit:]]' | xargs | tr -s ' ' ',')
endef

$(foreach FREETDS_VERSION, $(CHECKED_FREETDS_VERSIONS), $(eval $(call FREETDS_BUILD_RULE, $(FREETDS_VERSION))))

.PHONY: freetds-latest
freetds-latest: freetds-$(DEFAULT_FREETDS_VERSION)

.PHONY: pylint
pylint:
	tox -e $(@)

.PHONY: checkmetadata
checkmetadata:
	tox -e $(@)

.PHONY: docs
docs:
	tox -e $(@)
	echo "View generated documentation at: "
	echo "    file://$(abspath $(BUILDDIR))/docs/index.html"


.PHONY: check
check: checkmetadata pylint docs $(foreach FREETDS_VERSION, $(CHECKED_FREETDS_VERSIONS), test-$(FREETDS_VERSION))
