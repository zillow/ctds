ARG PYTHON_VERSION=3.6

FROM python:${PYTHON_VERSION}

ARG FREETDS_VERSION=1.00.40

# FreeTDS (required by ctds)
RUN set -ex \
    && curl -vfkLo freetds-${FREETDS_VERSION}.tar.gz "ftp://ftp.freetds.org/pub/freetds/stable/freetds-${FREETDS_VERSION}.tar.gz" \
    && mkdir -p /usr/src \
    && tar -xzC /usr/src -f freetds-${FREETDS_VERSION}.tar.gz \
    && rm freetds-${FREETDS_VERSION}.tar.gz \
    && cd /usr/src/freetds-${FREETDS_VERSION} \
    && ./configure \
        --disable-odbc \
        --disable-apps \
        --disable-server \
        --disable-pool \
        --datarootdir=/usr/src/freetds/data \
        --prefix=/usr \
    && make \
    && make install \
    && rm -rf \
       /usr/src/freetds-${FREETDS_VERSION} \
       /usr/src/freetds

RUN pip install --no-cache-dir \
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
