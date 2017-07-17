#!/bin/sh -e

# Start SQL Server
/opt/mssql/bin/sqlservr &

# Create TDS unit test tables.
/opt/mssql-tools/bin/sqlcmd -S localhost -U sa -P ${SA_PASSWORD} -d master -i test-setup.sql

wait
