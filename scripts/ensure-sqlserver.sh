#!/bin/sh -e

CONTAINER=${1:-ctds-unittest-sqlserver}

HOSTNAME=localhost
RETRIES=30

USERNAME=TDSUnittest
PASSWORD=TDSUnittest1

until docker exec "$CONTAINER" \
             /bin/sh -c "/opt/mssql-tools/bin/sqlcmd -S $HOSTNAME -U $USERNAME -P $PASSWORD -Q 'SELECT @@VERSION'"
do
    if [ -z `docker ps -f name=$CONTAINER -q` ]; then
        echo "MS SQL Server docker container not running; starting ..."
        docker build -q -f Dockerfile-sqlserver -t "$CONTAINER" .
        docker run -d \
               -e 'ACCEPT_EULA=Y' \
               -e 'SA_PASSWORD=cTDS-unitest!' \
               -e 'MSSQL_PID=Developer' \
               --rm \
               --name "$CONTAINER" \
               "$CONTAINER"
    fi
    if [ "$RETRIES" -le 0 ]; then
        echo "Retry count exceeded; exiting ..."
        exit 1
    fi
    RETRIES=$((RETRIES - 1))
    echo "$(date) waiting 1s for $CONTAINER to start ..."
    sleep 1s
done
