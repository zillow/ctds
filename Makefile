# -*- makefile-gmake -*-

PACKAGE_NAME = ctds

# Python version support
SUPPORTED_PYTHON_VERSIONS := \
    2.7 \
    3.3 \
    3.4 \
    3.5 \
    3.6

# FreeTDS versions to test against. This should
# be the latest of each minor release.
# Note: These version strings *must* match the versions
# on ftp://ftp.freetds.org/pub/freetds/stable/.
CHECKED_FREETDS_VERSIONS := \
    0.91.112 \
    0.92.405 \
    0.95.95 \
    1.00.55

DEFAULT_PYTHON_VERSION := $(lastword $(SUPPORTED_PYTHON_VERSIONS))
DEFAULT_FREETDS_VERSION := $(lastword $(CHECKED_FREETDS_VERSIONS))

CTDS_VERSION := $(strip $(shell python setup.py --version))

# Help
.PHONY: help
help:
	@echo "usage: make [check|clean|coverage|doc|publish|pylint|test]"
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
	@echo "    Optional variables:"
	@echo "      TEST - Optional test specifier. e.g. \`make test TEST=ctds.tests.test_tds_connect\`"


UNITTEST_DOCKER_IMAGE_NAME = ctds-unittest-python$(strip $(1))-$(strip $(2))
SQL_SERVER_DOCKER_IMAGE_NAME := ctds-unittest-sqlserver

GH_PAGES_DIR := $(abspath .gh-pages)

.PHONY: clean
clean: stop-sqlserver
	git clean -dfX
	docker images -q ctds-unittest-* | xargs -r docker rmi

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
	if [ `docker ps -f name=$(SQL_SERVER_DOCKER_IMAGE_NAME) -q` ]; then \
        docker stop $(SQL_SERVER_DOCKER_IMAGE_NAME); \
    fi

# Function to generate a rules for:
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
        --build-arg "FREETDS_VERSION=$(strip $(2))" \
        --build-arg "PYTHON_VERSION=$(strip $(1))" \
        -f Dockerfile \
        -t $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2)) \
        .

.PHONY: test_$(strip $(1))_$(strip $(2))
test_$(strip $(1))_$(strip $(2)): docker_$(strip $(1))_$(strip $(2)) start-sqlserver
	docker run --init --rm \
        -e DEBUG=1 \
        --network container:$(SQL_SERVER_DOCKER_IMAGE_NAME) \
        $(if $(TDSDUMP), -e TDSDUMP=$(TDSDUMP)) \
        $(if $(TEST), -e TEST=$(TEST)) \
        $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2)) \
        ./scripts/ctds-unittest.sh

.PHONY: coverage_$(strip $(1))_$(strip $(2))
coverage_$(strip $(1))_$(strip $(2)): docker_$(strip $(1))_$(strip $(2)) start-sqlserver
	docker run --init \
        -e CTDS_COVER=1 \
        -e DEBUG=1 \
        --workdir /usr/src/ctds/ \
        --network container:$(SQL_SERVER_DOCKER_IMAGE_NAME) \
        --name $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2))-coverage \
        $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2)) \
        ./scripts/ctds-coverage.sh
	mkdir -p $$@
	docker cp $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2))-coverage:/usr/src/ctds/coverage \
        $(abspath $$@)
	docker rm $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2))-coverage
endef

$(foreach PV, $(SUPPORTED_PYTHON_VERSIONS), $(foreach FV, $(CHECKED_FREETDS_VERSIONS), $(eval $(call GENERATE_RULES, $(PV), $(FV)))))

define CHECK_RULE
.PHONY: check_$(strip $(1))
check_$(strip $(1)): $(foreach FV, $(CHECKED_FREETDS_VERSIONS), coverage_$(strip $(1))_$(FV))
endef

$(foreach PV, $(SUPPORTED_PYTHON_VERSIONS), $(eval $(call CHECK_RULE, $(PV))))

define CHECKMETADATA_RULE
.PHONY: checkmetadata_$(strip $(1))
checkmetadata_$(strip $(1)): docker_$(strip $(1))_$(DEFAULT_FREETDS_VERSION)
	docker run --init --rm \
        --workdir /usr/src/ctds/ \
        $(call UNITTEST_DOCKER_IMAGE_NAME, $(strip $(1)), $(DEFAULT_FREETDS_VERSION)) \
        ./scripts/ctds-checkmetadata.sh
endef

$(foreach PV, $(SUPPORTED_PYTHON_VERSIONS), $(eval $(call CHECKMETADATA_RULE, $(PV))))

.PHONY: check
check: pylint $(foreach PV, $(SUPPORTED_PYTHON_VERSIONS), check_$(PV) checkmetadata_$(PV))

.PHONY: test
test: test_$(DEFAULT_PYTHON_VERSION)_$(DEFAULT_FREETDS_VERSION)

.PHONY: coverage
coverage: coverage_$(DEFAULT_PYTHON_VERSION)_$(DEFAULT_FREETDS_VERSION)

.PHONY: pylint
pylint: docker_$(DEFAULT_PYTHON_VERSION)_$(DEFAULT_FREETDS_VERSION)
	docker run --init --rm \
        --workdir /usr/src/ctds/ \
        $(call UNITTEST_DOCKER_IMAGE_NAME, $(DEFAULT_PYTHON_VERSION), $(DEFAULT_FREETDS_VERSION)) \
        ./scripts/ctds-pylint.sh

.PHONY: doc
doc: docker_$(DEFAULT_PYTHON_VERSION)_$(DEFAULT_FREETDS_VERSION)
	docker run --init \
        --workdir /usr/src/ctds/ \
        --name $(call UNITTEST_DOCKER_IMAGE_NAME, $(DEFAULT_PYTHON_VERSION), $(DEFAULT_FREETDS_VERSION))-doc \
        $(call UNITTEST_DOCKER_IMAGE_NAME, $(DEFAULT_PYTHON_VERSION), $(DEFAULT_FREETDS_VERSION)) \
        ./scripts/ctds-doc.sh "$(notdir $(GH_PAGES_DIR))"
	docker cp $(call UNITTEST_DOCKER_IMAGE_NAME, $(DEFAULT_PYTHON_VERSION), $(DEFAULT_FREETDS_VERSION))-doc:/usr/src/ctds/$(notdir $(GH_PAGES_DIR)) .
	docker rm $(call UNITTEST_DOCKER_IMAGE_NAME, $(DEFAULT_PYTHON_VERSION), $(DEFAULT_FREETDS_VERSION))-doc

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
