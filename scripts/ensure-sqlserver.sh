#!/bin/sh -e

CONTAINER=${1:-ctds-unittest-sqlserver}

HOSTNAME=localhost
RETRIES=30

USERNAME=TDSUnittest
PASSWORD=S0methingSecret!

if [ -z "`docker images -q \"$CONTAINER\"`" ]; then
    echo "Building MS SQL Server docker image ..."
    docker build -q -f Dockerfile-sqlserver -t "$CONTAINER" .
fi

CONTAINER_ID=`docker ps -a -f name="^/$CONTAINER$" -q`
if [ -z "$CONTAINER_ID" ]; then
    echo "MS SQL Server docker container not running; starting ..."
    CONTAINER_ID=`docker run -d \
           -e 'ACCEPT_EULA=Y' \
           -e 'SA_PASSWORD=cTDS-unitest123' \
           -e 'MSSQL_PID=Developer' \
           --name "$CONTAINER" \
           "$CONTAINER"`
fi

until docker exec "$CONTAINER_ID" \
             /bin/sh -c "/opt/mssql-tools/bin/sqlcmd -S $HOSTNAME -U $USERNAME -P $PASSWORD -W -b -Q 'SET NOCOUNT ON; SELECT @@VERSION'"
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
