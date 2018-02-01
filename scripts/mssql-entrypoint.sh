#!/bin/sh -e

# Start SQL Server
/opt/mssql/bin/sqlservr &

# Create TDS unit test tables.
until /opt/mssql-tools/bin/sqlcmd \
    -S localhost \
    -U sa \
    -P ${SA_PASSWORD} \
    -d master \
    -b -i test-setup.sql
do
    # On failure, wait a bit for SQL Server to finish starting.
    sleep 1
done

wait
