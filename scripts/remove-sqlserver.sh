#!/bin/sh -e

CONTAINER=${1:-ctds-unittest-sqlserver}

CONTAINER_ID=`docker ps -a -f name="^/$CONTAINER$" -q`

if [ -n "$CONTAINER_ID" ]; then
    echo "Cleaning up $CONTAINER ($CONTAINER_ID) ..."
    docker stop $CONTAINER_ID > /dev/null
    docker rm $CONTAINER_ID > /dev/null
fi
