# Change Log
All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).

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

[Unreleased]: https://github.com/zillow/ctds/compare/v1.2.1...HEAD
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
