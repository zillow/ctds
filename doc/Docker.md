# Installing FreeTDS and CTDS on Docker for python 3.6
Installing FreeTDS isn't as simple as using apt-get or any other package manager. Most repositories have old 
redundant versions of FreTDS.

We therefor need to build everything from scratch.

# Docker file content.

The docker file for building the docker image python 3.6 and any python app. A shell script (install_ctds.sh) handles building
FreeTDS with SSL and ctds.

```docker
FROM python:3.6-jessie

WORKDIR /usr/src/app
COPY install_ctds.sh ./
RUN ["/bin/bash", "-c", "./install_ctds.sh"]

COPY requirements.txt ./
RUN pip install --no-cache-dir -r requirements.txt

COPY . .

CMD my_startup_script.sh
```
## The shell script

Now the script that does all the work. Remeber to remove ctds from you requirements.txt file because it will be 
built by this script.

```sh
#!/bin/bash

apt-get install libc6-dev

wget 'ftp://ftp.freetds.org/pub/freetds/stable/freetds-patched.tar.gz'
tar -xzf freetds-patched.tar.gz
pushd freetds-*
./configure --prefix "$(dirname $(pwd))" --with-openssl='/usr/lib/ssl' && make && make install
popd

echo "FreeTDS Installation complete."

pip install --global-option=build_ext \
    --global-option="--include-dirs=$(pwd)/include" \
    --global-option=build_ext \
    --global-option="--library-dirs=$(pwd)/lib" \
    --global-option=build_ext --global-option="--rpath=./lib" \
    ctds

echo "CTDS installation complete."

exit 0
```
