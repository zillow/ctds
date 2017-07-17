#!/bin/sh -e

CONTAINER=${1:-"ctds-unittest-sqlserver"}

if [ -z `docker ps -f name=$CONTAINER -q` ]; then
    echo "MS SQL Server docker container not running; starting ..."
    docker build -f Dockerfile-sqlserver -t $CONTAINER .
    docker run -d \
        -e 'ACCEPT_EULA=Y' \
        -e 'SA_PASSWORD=cTDS-unitest!' \
        -e 'MSSQL_PID=Developer' \
        -p 1433:1433 \
        --rm \
        --name $CONTAINER \
        $CONTAINER
    # Wait for SQL Server to start.
    sleep 5s;
fi
