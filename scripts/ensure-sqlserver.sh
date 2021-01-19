#!/bin/sh -e

CONTAINER=${1:-ctds-unittest-sqlserver}

CURDIR=$(dirname $(cd -P -- "$(dirname -- "$0")" && printf '%s\n' "$(pwd -P)/$(basename -- "$0")"))

RETRIES=30

SA_PASSWORD=cTDS-unitest123

CONTAINER_ID=`docker ps -a -f name="^/$CONTAINER$" -q`
if [ -z "$CONTAINER_ID" ]; then
    echo "MS SQL Server docker container not running; starting ..."

    # Remove the container if it is stopped.
    CONTAINER_ID=`docker ps -a -f name="^/$CONTAINER$" -q`
    if [ -n "$CONTAINER_ID" ]; then
        docker rm $CONTAINER_ID
    fi

    CONTAINER_ID=`docker run -d \
           -e 'ACCEPT_EULA=Y' \
           -e "SA_PASSWORD=$SA_PASSWORD" \
           -e 'MSSQL_PID=Developer' \
           -p 1433:1433 \
           --name "$CONTAINER" \
           --hostname "$CONTAINER" \
           "mcr.microsoft.com/mssql/server:2017-latest"`
fi

until docker run \
        --rm \
        --network container:$CONTAINER_ID \
    mcr.microsoft.com/mssql-tools \
    /opt/mssql-tools/bin/sqlcmd -S localhost -U SA -P "$SA_PASSWORD" -W -b -Q 'SET NOCOUNT ON; SELECT @@VERSION'
do
    if [ "$RETRIES" -le 0 ]; then
        echo "Retry count exceeded; exiting ..."
        docker logs $CONTAINER_ID

        # On startup failure cleanup the container.
        docker rm $CONTAINER_ID
        exit 1
    fi
    RETRIES=$((RETRIES - 1))
    echo "$(date) waiting 1s for $CONTAINER ($CONTAINER_ID) to start ..."
    sleep 1
done

docker run \
        --rm \
        --network container:$CONTAINER_ID \
        -v $(dirname $CURDIR)/misc/:/misc \
    mcr.microsoft.com/mssql-tools \
    /opt/mssql-tools/bin/sqlcmd -S localhost -U SA -P "$SA_PASSWORD" -W -b -i "/misc/test-setup.sql"
