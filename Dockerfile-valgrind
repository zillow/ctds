FROM buildpack-deps:stretch

ARG PYTHON_VERSION=3.6.3
ARG FREETDS_VERSION=1.00.40

#
# Build Python from source for use with valgrind.
#

# http://bugs.python.org/issue19846
# > At the moment, setting "LANG=C" on a Linux system *fundamentally breaks Python 3*, and that's not OK.
ENV LANG C.UTF-8

# Runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    tcl \
    tk \
    valgrind \
    && rm -rf /var/lib/apt/lists/*

RUN set -ex \
    && buildDeps=' \
        dpkg-dev \
        tcl-dev \
        tk-dev \
    ' \
    && apt-get update && apt-get install -y $buildDeps --no-install-recommends && rm -rf /var/lib/apt/lists/* \
    \
    && wget -O python.tar.xz "https://www.python.org/ftp/python/${PYTHON_VERSION%%[a-z]*}/Python-$PYTHON_VERSION.tar.xz" \
    && mkdir -p /usr/src/python \
    && tar -xJC /usr/src/python --strip-components=1 -f python.tar.xz \
    && rm python.tar.xz \
    \
    && cd /usr/src/python \
    && gnuArch="$(dpkg-architecture --query DEB_BUILD_GNU_TYPE)" \
    && ./configure \
        --build="$gnuArch" \
        --without-pymalloc \
        --with-pydebug \
        --with-valgrind \
        --enable-shared \
        --with-system-expat \
        --with-system-ffi \
    && make -j "$(nproc)" \
    && make install \
    && ldconfig \
    \
    && apt-get purge -y --auto-remove $buildDeps \
    \
    && find /usr/local -depth \
        \( \
            \( -type d -a \( -name test -o -name tests \) \) \
            -o \
            \( -type f -a \( -name '*.pyc' -o -name '*.pyo' \) \) \
        \) -exec rm -rf '{}' + \
    && rm -rf /usr/src/python

# FreeTDS (required by ctds)
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

COPY . /usr/src/ctds/
WORKDIR /usr/src/ctds

CMD ["/bin/bash"]
