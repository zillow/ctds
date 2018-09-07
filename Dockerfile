ARG PYTHON_VERSION=3.6

FROM python:${PYTHON_VERSION}

ARG FREETDS_VERSION=1.00.80

# Build FreeTDS (required by ctds)
RUN set -ex \
    && wget -O freetds.tar.gz "http://www.freetds.org/files/stable/freetds-${FREETDS_VERSION}.tar.gz" \
    && mkdir -p /usr/src/freetds \
    && tar -xzC /usr/src/freetds --strip-components=1 -f freetds.tar.gz \
    && rm freetds.tar.gz \
    && cd /usr/src/freetds \
    && ./configure \
        --disable-odbc \
        --disable-apps \
        --disable-server \
        --disable-pool \
        --datarootdir=/usr/src/freetds/data \
        --prefix=/usr \
    && make -j "$(nproc)" \
    && make install \
    && rm -rf \
       /usr/src/freetds

# Python 3.3 support requires specific versions.
RUN set -ex \
    && case "${PYTHON_VERSION}" in 3.3*) \
        pip --disable-pip-version-check \
            --no-cache-dir \
            install \
                --no-deps \
                isort==4.2.5 \
                sphinx==1.4.9; \
    esac

RUN set -ex \
    && pip --disable-pip-version-check \
            --no-cache-dir \
        install \
            --upgrade \
            pip \
    && pip --no-cache-dir \
        install \
            check-manifest \
            coverage \
            docutils \
            pylint \
            recommonmark \
            sphinx \
            sphinx_rtd_theme

COPY . /usr/src/ctds/
WORKDIR /usr/src/ctds

CMD ["/bin/bash"]
