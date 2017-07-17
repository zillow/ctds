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
    1.00.40

DEFAULT_PYTHON_VERSION := $(lastword $(SUPPORTED_PYTHON_VERSIONS))
DEFAULT_FREETDS_VERSION := $(lastword $(CHECKED_FREETDS_VERSIONS))

VALGRIND_FLAGS := \
    --leak-check=summary \
    --fullpath-after=/ \
    --num-callers=24 \
    --malloc-fill=0xa0 \
    --free-fill=0xff \
    --log-file=valgrind.log

CTDS_VERSION := $(strip $(shell python setup.py --version))

# Help
.PHONY: help
help:
	@echo "usage: make [check|clean|cover|pylint|test]"
	@echo
	@echo "    check"
	@echo "        Run tests against all supported versions of Python and"
	@echo "        the following versions of FreeTDS: $(CHECKED_FREETDS_VERSIONS)."
	@echo
	@echo "    clean"
	@echo "        Clean source tree."
	@echo
	@echo "    cover"
	@echo "        Generate code coverage for c and Python source code."
	@echo
	@echo "    doc"
	@echo "        Generate documentation."
	@echo
	@echo "    publish"
	@echo "        Publish egg and docs to pypi."
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
	@echo "      VALGRIND - Run tests under \`valgrind\`. e.g. \`make test VALGRIND=1\`"


# clean
.PHONY: clean
clean:
	git clean -dfX

# doc
$(eval $(call ENV_RULE, doc, $(DEFAULT_PYTHON_VERSION), $(DEFAULT_FREETDS_VERSION), sphinx sphinx_rtd_theme, DOC_COMMANDS))

publish: doc
	git tag -a v$(CTDS_VERSION) -m "v$(CTDS_VERSION)"
	git push --tags
	python setup.py sdist upload
	python setup.py upload_docs --upload-dir=$(HTML_BUILD_DIR)


UNITTEST_DOCKER_IMAGE_NAME = ctds-unittest-python$(strip $(1))-$(strip $(2))
SQL_SERVER_DOCKER_IMAGE_NAME := ctds-unittest-sqlserver

.PHONY: start-sqlserver
start-sqlserver:
	@if [ -z `docker ps -f name=$(SQL_SERVER_DOCKER_IMAGE_NAME) -q` ]; then \
        echo "MS SQL Server docker container not running; starting ..."; \
        docker build $(if $(VERBOSE),,-q) -f Dockerfile-sqlserver -t $(SQL_SERVER_DOCKER_IMAGE_NAME) .; \
        docker run -d \
            -e 'ACCEPT_EULA=Y' \
            -e 'SA_PASSWORD=cTDS-unitest!' \
            -e 'MSSQL_PID=Developer' \
            --rm \
            --name $(SQL_SERVER_DOCKER_IMAGE_NAME) \
            $(SQL_SERVER_DOCKER_IMAGE_NAME); \
        sleep 5s; \
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
        -f Dockerfile-unittest \
        -t $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2)) \
        .

.PHONY: test_$(strip $(1))_$(strip $(2))
test_$(strip $(1))_$(strip $(2)): docker_$(strip $(1))_$(strip $(2)) start-sqlserver
	docker run --init --rm \
        -e DEBUG=1 \
        --network container:$(SQL_SERVER_DOCKER_IMAGE_NAME) \
        $(if $(TDSDUMP), -e TDSDUMP=$(TDSDUMP)) \
        $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2)) \
        "./scripts/ctds-unittest.sh"

.PHONY: coverage_$(strip $(1))_$(strip $(2))
coverage_$(strip $(1))_$(strip $(2)): docker_$(strip $(1))_$(strip $(2)) start-sqlserver
	docker run --init \
        -e CTDS_COVER=1 \
        -e DEBUG=1 \
        --workdir /usr/src/ctds/ \
        --network container:$(SQL_SERVER_DOCKER_IMAGE_NAME) \
        --name $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2))-coverage \
        $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2)) \
        "./scripts/ctds-coverage.sh"
	docker cp $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2))-coverage:/usr/src/ctds/coverage $(abspath coverage)
	docker rm $(call UNITTEST_DOCKER_IMAGE_NAME, $(1), $(2))-coverage
endef

$(foreach PV, $(SUPPORTED_PYTHON_VERSIONS), $(foreach FV, $(CHECKED_FREETDS_VERSIONS), $(eval $(call GENERATE_RULES, $(PV), $(FV)))))

define CHECK_RULE
.PHONY: check_$(strip $(1))
check_$(strip $(1)): $(foreach FV, $(CHECKED_FREETDS_VERSIONS), test_$(strip $(1))_$(FV))
endef

$(foreach PV, $(SUPPORTED_PYTHON_VERSIONS), $(eval $(call CHECK_RULE, $(PV))))

.PHONY: check
check: pylint $(foreach PV, $(SUPPORTED_PYTHON_VERSIONS), check_$(PV))

.PHONY: test
test: test_$(DEFAULT_PYTHON_VERSION)_$(DEFAULT_FREETDS_VERSION)

.PHONY: coverage
coverage: coverage_$(DEFAULT_PYTHON_VERSION)_$(DEFAULT_FREETDS_VERSION)

.PHONY: pylint
pylint: docker_$(DEFAULT_PYTHON_VERSION)_$(DEFAULT_FREETDS_VERSION)
	docker run --init --rm \
        --workdir /usr/src/ctds/ \
        $(call UNITTEST_DOCKER_IMAGE_NAME, $(DEFAULT_PYTHON_VERSION), $(DEFAULT_FREETDS_VERSION)) \
        "./scripts/ctds-pylint.sh"

.PHONY: doc
doc: docker_$(DEFAULT_PYTHON_VERSION)_$(DEFAULT_FREETDS_VERSION)
	docker run --init \
        --workdir /usr/src/ctds/ \
        --name $(call UNITTEST_DOCKER_IMAGE_NAME, $(DEFAULT_PYTHON_VERSION), $(DEFAULT_FREETDS_VERSION))-doc \
        $(call UNITTEST_DOCKER_IMAGE_NAME, $(DEFAULT_PYTHON_VERSION), $(DEFAULT_FREETDS_VERSION)) \
        "./scripts/ctds-doc.sh"
	docker cp $(call UNITTEST_DOCKER_IMAGE_NAME, $(DEFAULT_PYTHON_VERSION), $(DEFAULT_FREETDS_VERSION))-doc:/usr/src/ctds/docs $(abspath docs)
	docker rm $(call UNITTEST_DOCKER_IMAGE_NAME, $(DEFAULT_PYTHON_VERSION), $(DEFAULT_FREETDS_VERSION))-doc
