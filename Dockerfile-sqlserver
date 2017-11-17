FROM microsoft/mssql-server-linux:2017-latest

COPY scripts/mssql-entrypoint.sh /

COPY misc/test-setup.sql /

EXPOSE 1433

CMD ["/mssql-entrypoint.sh"]
