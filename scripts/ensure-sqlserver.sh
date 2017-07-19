#!/bin/sh -e

CONTAINER=${1:-ctds-unittest-sqlserver}

if [ -z `docker ps -f name=$CONTAINER -q` ]; then \
    echo "MS SQL Server docker container not running; starting ..."; \
    docker build -q -f Dockerfile-sqlserver -t "$CONTAINER" .; \
    docker run -d \
           -e 'ACCEPT_EULA=Y' \
           -e 'SA_PASSWORD=cTDS-unitest!' \
           -e 'MSSQL_PID=Developer' \
           -p 1433:1433 \
           --rm \
           --name "$CONTAINER" \
           "$CONTAINER"
fi

HOSTNAME=localhost
PORT=1433
RETRIES=10

USERNAME=TDSUnittest
PASSWORD=TDSUnittest1

until docker exec "$CONTAINER" \
             /bin/sh -c "/opt/mssql-tools/bin/sqlcmd -S $HOSTNAME -U $USERNAME -P $PASSWORD -Q 'SELECT @@VERSION'"
do
    if [ "$RETRIES" -le 0 ]; then
        echo "Retry count $RETRIES exceeded; exiting ..."
        exit 1
    fi
    retries=$((retries - 1))
    echo "$(date) waiting 5s for $HOSTNAME:$PORT to start ..."
    sleep 5s
done
