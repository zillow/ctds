# Change Log
All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased]
## [1.0.7] - 2016-08-15
### Fixed
- Only use strict compile options for debug/development builds. Allow
the client (e.g. pip) specify the desired options when installing as an
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

[Unreleased]: https://github.com/zillow/ctds/compare/v1.0.5...HEAD
[1.0.4]: https://github.com/zillow/ctds/compare/v1.0.4...v1.0.5
[1.0.4]: https://github.com/zillow/ctds/compare/v1.0.3...v1.0.4
[1.0.3]: https://github.com/zillow/ctds/compare/v1.0.2...v1.0.3
[1.0.2]: https://github.com/zillow/ctds/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/zillow/ctds/compare/v1.0...v1.0.1
