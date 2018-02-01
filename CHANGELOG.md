# Change Log
All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased]
### Fixed
- Retry on SQL Server unittest database setup failures due to race conditions when
starting SQL Server. This should make the unit tests much more reliable.
- Add `python_requires` specifier to *setup.py*.

## [1.7.0] - 2018-01-24
### Added
- Add rich comparison support and `repr` to `ctds.Parameter`.

### Fixed
- Compiler warnings on older version of gcc.
- Debug assert when passing invalid parameters to `ctds.Cursor.execute*()`.

## [1.6.3] - 2017-12-05
### Fixed
- Raise proper warning from `ctds.Connection.messages`.

## [1.6.2] - 2017-11-27
### Fixed
- Support values longer than 8000 characters in `ctds.SqlVarChar`.

## [1.6.1] - 2017-11-20
### Fixed
- Revert passing BINARY 0x00 for (N)VARCHAR arguments.

## [1.6.0] - 2017-11-17
### Added
- Documentation improvements.
- Add an optional *paramstyle* argument to `ctds.connect`, with support
for the `named` *paramstyle*.
- Add valgrind support for memory leak detection and integrate it into the CI
build process (Travis only).
- Add optional `read_only` argument to `ctds.connect` for indication of
read-only intent. Note: Requires as yet unreleased FreeTDS support.
### Fixed
- Pass a BINARY 0x00 byte instead of an empty string for (N)VARCHAR arguments.
This is to work around the inability of the db-lib API to pass empty string
parameters via RPC.

## [1.5.0] - 2017-10-16
### Added
- Windows support, with Appveyor continuous integration.
- Python egg-related metadata checks.
- Moved source code under ./src directory, per best practices.

## [1.4.1] - 2017-09-29
### Fixed
- Only clear `Connection.messages` on calls to `execute`, `executemany`
or `callproc`. Add support for raising multiple non-zero severity
messages as warnings.

## [1.4.0] - 2017-09-13
### Fixed
- Changed the implementation of `Cursor.execute` to only perform
parameter substitution if parameters are provided. When no parameters
are passed, the SQL format string is treated as raw SQL.
### Added
- `Connection.messages` read-only property.
- Improved code coverage.

## [1.3.2] - 2017-07-25
### Fixed
- Register RowListType to support pickling/unpickling.
### Added
- Convert test framework to use Docker-based containers for
SQL Server and for each supported combination of Python and FreeTDS.
- Integrate [codecov](https://codecov.io/) support for code coverage
tracking.
- Move documentation hosting to [GitHub pages](https://pages.github.com/).
- Include change log in generated documentation.

## [1.3.1] - 2017-05-31
### Fixed
- Fix connection failures when connecting to high port numbers on OS X.
- Fix unit test infrastructure to respect configured port number.
- Work around inconsistencies between FreeTDS versions in the bcp_getl/
BCP_SETL interfaces.

## [1.3.0] - 2017-03-15
### Fixed
- Replace usage of the deprecated `PyErr_Warn` API with `PyErr_WarnEx`.
This will allow clients to indicate that warnings should be raised as
exceptions.

## [1.2.3] - 2017-03-13
### Fixed
- Improve `repr` implementation for SQL type wrapper objects.
- Fixed informational (warning) message reporting where many SQL Server
reported messages were not reported as warnings due if not returned to
the client promptly enough.

## [1.2.2] - 2017-01-10
### Fixed
- Fix multi-byte UTF-16 character conversion.

## [1.2.1] - 2016-12-27
### Fixed
- Include `ctds.pool` in egg package.

## [1.2.0] - 2016-12-27
### Fixed
- Documentation improvements around *(N)VARCHAR* handling.

### Added
- `ctds.pool.ConnectionPool` class.
- Support the `in` keyword for resultset rows.
- Add code coverage support for Python code.
- Improve code coverage support for c code.

## [1.1.0] - 2016-11-14
### Added
- `ctds.SqlNVarChar` type.
- Ignore the replacement parameter markers inside of string literals in
SQL statements passed to `ctds.Cursor.execute()` and
`ctds.Cursor.executemany()`.

### Fixed
- Work around a Memory leak in FreeTDS when specifying the database at
connection time.
- Fixed failure in `ctds.Cursor.execute()` and `ctds.Cursor.executemany()`
when passing a SQL statement longer than 4000 characters.

## [1.0.8] - 2016-08-17
### Fixed
- Configure connections to use UTF-16 when supported (i.e. FreeTDS 1.00+).
Don't replace codepoints outside of the UCS-2 range if the connection is
using UTF-16 as it is no longer necessary and causes data loss.

## [1.0.7] - 2016-08-15
### Fixed
- Only use strict compile options for debug/development builds. Allow
the client (e.g. pip) to specify the desired options when installing as an
egg.

## [1.0.6] - 2016-08-15
### Fixed
- Compile under the *c90* standard for better future portability.
- Don't overwrite more useful error messages with the useless "The statement
has been terminated." error.

### Added
- Minor documentation improvements.

## [1.0.5] - 2016-05-23
### Fixed
- Fix the `ctds.Connection.timeout` property to raise an appropriate error
when the option is supported, but the set fails.

## [1.0.4] - 2016-05-19
### Fixed
- Fix the `ctds.Connection.timeout` property to work with the **DBSETTIME**
option added in [FreeTDS](http://freetds.org) version *1.00*.

### Added
- Support for TDS version *7.4*, introduced with [FreeTDS](http://freetds.org)
version *1.00*.

## [1.0.3] - 2016-03-31
### Fixed
- Fix [execute()](https://www.python.org/dev/peps/pep-0249/#execute) and
[executemany()](https://www.python.org/dev/peps/pep-0249/#executemany) to work
with older versions of [FreeTDS](http://freetds.org) (prior to 0.92.405).
Format the SQL command manually, instead of using **sp_executesql**, since
older versions of [FreeTDS](http://freetds.org) don't seem to support passing
_NVARCHAR_ arguments to remote procedure calls.

### Added
- Transaction documentation
- Bulk insert documentation

## [1.0.2] - 2016-03-15
### Fixed
- Documentation typos and improvements

### Added
- [travis-ci](https://travis-ci.org/zillow/ctds) integration

## [1.0.1] - 2016-03-14
### Fixed
- Build on OS X

### Added
- Hypothetical integration with readthdocs.org

## [1.0.0] - 2016-03-14
Initial Release

[Unreleased]: https://github.com/zillow/ctds/compare/v1.7.0...HEAD
[1.7.0]: https://github.com/zillow/ctds/compare/v1.7.0...v1.6.3
[1.6.3]: https://github.com/zillow/ctds/compare/v1.6.3...v1.6.2
[1.6.2]: https://github.com/zillow/ctds/compare/v1.6.2...v1.6.1
[1.6.1]: https://github.com/zillow/ctds/compare/v1.6.1...v1.6.0
[1.6.0]: https://github.com/zillow/ctds/compare/v1.6.0...v1.5.0
[1.5.0]: https://github.com/zillow/ctds/compare/v1.5.0...v1.4.1
[1.4.1]: https://github.com/zillow/ctds/compare/v1.4.1...v1.4.0
[1.4.0]: https://github.com/zillow/ctds/compare/v1.4.0...v1.3.2
[1.3.2]: https://github.com/zillow/ctds/compare/v1.3.2...v1.3.1
[1.3.1]: https://github.com/zillow/ctds/compare/v1.3.1...v1.3.0
[1.3.0]: https://github.com/zillow/ctds/compare/v1.3.0...v1.2.3
[1.2.3]: https://github.com/zillow/ctds/compare/v1.2.3...v1.2.2
[1.2.2]: https://github.com/zillow/ctds/compare/v1.2.2...v1.2.1
[1.2.1]: https://github.com/zillow/ctds/compare/v1.2.1...v1.2.0
[1.2.0]: https://github.com/zillow/ctds/compare/v1.2.0...v1.1.0
[1.1.0]: https://github.com/zillow/ctds/compare/v1.1.0...v1.0.8
[1.0.8]: https://github.com/zillow/ctds/compare/v1.0.8...v1.0.7
[1.0.7]: https://github.com/zillow/ctds/compare/v1.0.7...v1.0.6
[1.0.6]: https://github.com/zillow/ctds/compare/v1.0.6...v1.0.5
[1.0.5]: https://github.com/zillow/ctds/compare/v1.0.5...v1.0.4
[1.0.4]: https://github.com/zillow/ctds/compare/v1.0.4...v1.0.5
[1.0.4]: https://github.com/zillow/ctds/compare/v1.0.3...v1.0.4
[1.0.3]: https://github.com/zillow/ctds/compare/v1.0.2...v1.0.3
[1.0.2]: https://github.com/zillow/ctds/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/zillow/ctds/compare/v1.0...v1.0.1
[1.0.0]: https://github.com/zillow/ctds/releases/tag/v1.0
