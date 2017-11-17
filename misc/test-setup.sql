/*
    Simple script to create a database, login and user for cTDS unit testing.

    Set @Database, @Username, and @Password as desired.
*/

USE master;
GO

-- Database name. It intentionally exceeds 30 characters for testing purposes.
DECLARE @Database NVARCHAR(128); -- Note: must also be set in the USE statement below.
SET @Database = N'TDSUnittest_12345678901234567890';

/* Create the database if it doesn''t already exist. */
IF NOT EXISTS(SELECT 1 FROM sys.databases WHERE name = @Database)
    BEGIN
        DECLARE @Template NVARCHAR(MAX);
        SET @Template = N'CREATE DATABASE {DATABASE}';
        SET @Template = REPLACE(@Template, '{DATABASE}', @Database)
        EXEC (@Template);
        PRINT 'Created database "' + @Database + '".';
    END
ELSE
    BEGIN
        PRINT 'Database "' + @Database + '" already exists.';
    END

GO

-- Username/password
DECLARE @Login NVARCHAR(128);
SET @Login = N'TDSUnittest';

DECLARE @Password NVARCHAR(128);
-- Use a password which meets default complexity rules.
SET @Password = N'S0methingSecret!';

IF NOT EXISTS(SELECT 1 FROM sys.server_principals WHERE name = @Login)
    BEGIN
        DECLARE @Template NVARCHAR(MAX);
        SET @Template = N'CREATE LOGIN {LOGIN} WITH PASSWORD = ''{PASSWORD}'', DEFAULT_DATABASE = {DATABASE}';
        SET @Template = REPLACE(@Template, '{DATABASE}', DB_NAME());
        SET @Template = REPLACE(@Template, '{LOGIN}', @Login);
        SET @Template = REPLACE(@Template, '{PASSWORD}', @Password);
        EXEC (@Template);
        PRINT 'Created login "' + @Login + '".';
    END
ELSE
    BEGIN
        PRINT 'Login "' + @Login + '" already exists.';
    END
GO

USE TDSUnittest_12345678901234567890;
GO

DECLARE @Username NVARCHAR(128);
SET @Username = N'TDSUnittest';

IF NOT EXISTS(SELECT 1 FROM sys.database_principals WHERE name = @Username)
    BEGIN
        DECLARE @Template NVARCHAR(MAX);
        SET @Template = N'
CREATE USER {USERNAME} FOR LOGIN {USERNAME};
EXEC sp_addrolemember ''db_owner'', ''{USERNAME}'';';
        SET @Template = REPLACE(@Template, '{DATABASE}', DB_NAME());
        SET @Template = REPLACE(@Template, '{USERNAME}', @Username);
        EXEC (@Template);
        PRINT 'Created User "' + @Username + '" in database "' + DB_NAME() + '".';
    END
ELSE
    BEGIN
        PRINT 'User "' + @Username + '" already exists for database "' + DB_NAME() + '".';
    END

GO
USE master;
GO
