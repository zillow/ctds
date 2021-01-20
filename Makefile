# -*- makefile-gmake -*-

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


DEFAULT_PYTHON_VERSION := $(lastword $(SUPPORTED_PYTHON_VERSIONS))
DEFAULT_FREETDS_VERSION := $(lastword $(CHECKED_FREETDS_VERSIONS))

VIRTUALENV_FREETDS_VERSION ?= $(lastword $(CHECKED_FREETDS_VERSIONS))

# Ignore warnings, i.e. "Unknown distribution option: 'python_requires'"
CTDS_VERSION := $(strip $(shell python -W ignore setup.py --version))

COVERAGE_DIR ?= .coverage

# Help
.PHONY: help
help:
	@echo "usage: make [check|clean|coverage|doc|publish|pylint|test|valgrind|virtualenv]"
	@echo
	@echo "    check"
	@echo "        Run tests against all supported versions of Python and"
	@echo "        the following versions of FreeTDS: $(CHECKED_FREETDS_VERSIONS)."
	@echo
	@echo "    clean"
	@echo "        Clean source tree."
	@echo
	@echo "    coverage"
	@echo "        Generate code coverage for c and Python source code."
	@echo
	@echo "    doc"
	@echo "        Generate documentation."
	@echo
	@echo "    publish"
	@echo "        Publish egg to pypi."
	@echo
	@echo "    pylint"
	@echo "        Run pylint over all *.py files."
	@echo
	@echo "    test"
	@echo "        Run tests using the default Python version ($(DEFAULT_PYTHON_VERSION)) and"
	@echo "        the default version of FreeTDS ($(DEFAULT_FREETDS_VERSION))."
	@echo
	@echo "    valgrind"
	@echo "        Run tests using a debug build of Python versions ($(VALGRIND_PYTHON_VERSIONS)) and"
	@echo "        FreeTDS versions ($(VALGRIND_FREETDS_VERSIONS)) under valgrind."
	@echo
	@echo "    Optional variables:"
	@echo "      TEST - Optional test specifier. e.g. \`make test TEST=ctds.tests.test_tds_connect\`"
	@echo "      VERBOSE - Include more verbose output."

DARWIN := $(findstring Darwin,$(shell uname))

UNITTEST_DOCKER_IMAGE_NAME = ctds-unittest-python$(strip $(1))-$(strip $(2))
SQL_SERVER_DOCKER_IMAGE_NAME := ctds-unittest-sqlserver
VALGRIND_DOCKER_IMAGE_NAME = ctds-valgrind-python$(strip $(1))-$(strip $(2))

DOCKER_ENV = \
    -e DEBUG=1 \
    $(if $(TDSDUMP),-e TDSDUMP=$(TDSDUMP)) \
    $(if $(TEST),-e TEST=$(TEST)) \
    $(if $(VERBOSE),-e VERBOSE=$(VERBOSE))

define DOCKER_RM
	@if [ ! -z `docker ps -a -f name=$(strip $(1)) -q` ]; then \
        docker rm $(strip $(1)); \
    fi
endef

GH_PAGES_DIR := $(abspath .gh-pages)

.PHONY: clean
clean: stop-sqlserver
	git clean -dfX
	docker images -q ctds-unittest-* | xargs $(if $(DARWIN),,-r) docker rmi

.PHONY: publish
publish:
	git tag -a v$(CTDS_VERSION) -m "v$(CTDS_VERSION)"
	git push --tags
	python setup.py sdist upload


.PHONY: start-sqlserver
start-sqlserver:
	scripts/ensure-sqlserver.sh $(SQL_SERVER_DOCKER_IMAGE_NAME)

.PHONY: stop-sqlserver
stop-sqlserver:
	scripts/remove-sqlserver.sh $(SQL_SERVER_DOCKER_IMAGE_NAME)

$(COVERAGE_DIR):
	mkdir -p $@

# Function to generate rules for:
#   * building a docker image with a specific Python/FreeTDS version
#   * running unit tests for a specific Python/FreeTDS version
#   * running code coverage for a specific Python/FreeTDS version
#
# $(eval $(call GENERATE_RULES, <python_version>, <freetds_version>))
#
define GENERATE_RULES
.PHONY: docker_$(strip $(1))_$(strip $(2))
docker_$(strip $(1))_$(strip $(2)):
	docker build $(if $(VERBOSE),,-q) \
        --build-arg "PYTHON_VERSION=$(strip $(1))" \
        --build-arg "FREETDS_VERSION=$(strip $(2))" \
        -f Dockerfile \
        -t $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2)) \
        .

.PHONY: test_$(strip $(1))_$(strip $(2))
test_$(strip $(1))_$(strip $(2)): docker_$(strip $(1))_$(strip $(2)) start-sqlserver
	docker run --init --rm -it \
        $(DOCKER_ENV) \
        --network container:$(SQL_SERVER_DOCKER_IMAGE_NAME) \
        $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2)) \
        ./scripts/ctds-unittest.sh

.PHONY: test_$(strip $(1))
test_$(strip $(1)): test_$(strip $(1))_$(DEFAULT_FREETDS_VERSION)

.PHONY: coverage_$(strip $(1))_$(strip $(2))
coverage_$(strip $(1))_$(strip $(2)): docker_$(strip $(1))_$(strip $(2)) start-sqlserver | $(COVERAGE_DIR)
	$(call DOCKER_RM, $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2))-coverage)
	docker run --init -it \
        -e CTDS_COVER=1 \
        $(DOCKER_ENV) \
        --name $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2))-coverage \
        --network container:$(SQL_SERVER_DOCKER_IMAGE_NAME) \
        $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2)) \
        ./scripts/ctds-coverage.sh
	docker cp $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2))-coverage:/usr/src/ctds/coverage \
        $(abspath $(COVERAGE_DIR)/$$@)
	docker rm $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2))-coverage
endef

$(foreach PV, $(SUPPORTED_PYTHON_VERSIONS), $(foreach FV, $(CHECKED_FREETDS_VERSIONS), $(eval $(call GENERATE_RULES, $(PV), $(FV)))))


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
	docker run --init --rm -it \
        $(DOCKER_ENV) \
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


define CHECK_RULE
.PHONY: check_$(strip $(1))
check_$(strip $(1)): checkmetadata_$(strip $(1)) $(foreach FV, $(CHECKED_FREETDS_VERSIONS), coverage_$(strip $(1))_$(FV))
endef

$(foreach PV, $(SUPPORTED_PYTHON_VERSIONS), $(eval $(call CHECK_RULE, $(PV))))

define CHECKMETADATA_RULE
.PHONY: checkmetadata_$(strip $(1))
checkmetadata_$(strip $(1)): docker_$(strip $(1))_$(DEFAULT_FREETDS_VERSION)
	docker run --init --rm -it \
        $(DOCKER_ENV) \
        $(call UNITTEST_DOCKER_IMAGE_NAME, $(strip $(1)), $(DEFAULT_FREETDS_VERSION)) \
        ./scripts/ctds-checkmetadata.sh
endef

$(foreach PV, $(SUPPORTED_PYTHON_VERSIONS), $(eval $(call CHECKMETADATA_RULE, $(PV))))

.PHONY: check
check: pylint $(foreach PV, $(CHECKED_PYTHON_VERSIONS), check_$(PV))

.PHONY: test
test: test_$(DEFAULT_PYTHON_VERSION)_$(DEFAULT_FREETDS_VERSION)

.PHONY: valgrind
valgrind: $(foreach PV, $(VALGRIND_PYTHON_VERSIONS), valgrind_$(PV))

.PHONY: coverage
coverage: coverage_$(DEFAULT_PYTHON_VERSION)_$(DEFAULT_FREETDS_VERSION)

.PHONY: pylint
pylint: docker_$(DEFAULT_PYTHON_VERSION)_$(DEFAULT_FREETDS_VERSION)
	docker run --init --rm -it \
        $(call UNITTEST_DOCKER_IMAGE_NAME, $(DEFAULT_PYTHON_VERSION), $(DEFAULT_FREETDS_VERSION)) \
        ./scripts/ctds-pylint.sh

.PHONY: doc
doc: docker_$(DEFAULT_PYTHON_VERSION)_$(DEFAULT_FREETDS_VERSION)
	$(call DOCKER_RM, $(call UNITTEST_DOCKER_IMAGE_NAME, $(DEFAULT_PYTHON_VERSION), $(DEFAULT_FREETDS_VERSION))-doc)
	docker run --init -it \
        --name $(call UNITTEST_DOCKER_IMAGE_NAME, $(DEFAULT_PYTHON_VERSION), $(DEFAULT_FREETDS_VERSION))-doc \
        $(call UNITTEST_DOCKER_IMAGE_NAME, $(DEFAULT_PYTHON_VERSION), $(DEFAULT_FREETDS_VERSION)) \
        ./scripts/ctds-doc.sh "$(notdir $(GH_PAGES_DIR))"
	docker cp $(call UNITTEST_DOCKER_IMAGE_NAME, $(DEFAULT_PYTHON_VERSION), $(DEFAULT_FREETDS_VERSION))-doc:/usr/src/ctds/$(notdir $(GH_PAGES_DIR)) .
	docker rm $(call UNITTEST_DOCKER_IMAGE_NAME, $(DEFAULT_PYTHON_VERSION), $(DEFAULT_FREETDS_VERSION))-doc
ifndef CI
	@echo; \
    echo "View generated documentation at: "; \
    echo "    file://$(GH_PAGES_DIR)/index.html"; \
    echo;
endif

.PHONY: _pre_publish-doc
_pre_publish-doc:
	rm -rf "$(GH_PAGES_DIR)"
	git clone --quiet --branch=gh-pages git@github.com:zillow/ctds.git "$(GH_PAGES_DIR)"

.PHONY: _post_publish-doc
_post_publish-doc:
	@if [ -n "`git -C "$(GH_PAGES_DIR)" status -s`" ]; then \
        echo; \
        echo "The ctds documentation has changed and should be re-published using: "; \
        echo "    git -C "$(GH_PAGES_DIR)" commit -am \"Documentation updates for ctds $(CTDS_VERSION)\""; \
        echo; \
        git -C "$(GH_PAGES_DIR)" status; \
    fi

.PHONY: publish-doc
publish-doc: _pre_publish-doc doc _post_publish-doc


BUILDDIR ?= build

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
	cd $(BUILDDIR)/src/freetds-$(strip $(1)) \
    && ./configure --prefix "$(abspath $(BUILDDIR)/freetds-$(strip $(1)))" \
    && make \
    && make install

.PHONY: freetds-$(strip $(1))
freetds-$(strip $(1)): $(BUILDDIR)/freetds-$(strip $(1))/include/sybdb.h

.PHONY: test-$(strip $(1))
test-$(strip $(1)): freetds-$(strip $(1))
	CTDS_INCLUDE_DIRS="$(abspath $(BUILDDIR)/freetds-$(strip $(1)))/include" \
    CTDS_LIBRARY_DIRS="$(abspath $(BUILDDIR)/freetds-$(strip $(1)))/lib" \
    CTDS_RUNTIME_LIBRARY_DIRS="$(abspath $(BUILDDIR)/freetds-$(strip $(1)))/lib" \
    tox -vv $$$$(tox -l | grep 'py[[:digit:]]' | sed -e 's/ /,/g')
endef

$(foreach FREETDS_VERSION, $(CHECKED_FREETDS_VERSIONS), $(eval $(call FREETDS_BUILD_RULE, $(FREETDS_VERSION))))

.PHONY: freetds-latest
freetds-latest: freetds-$(DEFAULT_FREETDS_VERSION)
