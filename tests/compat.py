import sys

PY3 = sys.version_info[0] >= 3
PY33 = sys.version_info >= (3, 3)
PY34 = sys.version_info >= (3, 4)
PY35 = sys.version_info >= (3, 5)
PY36 = sys.version_info >= (3, 6)
PY27 = sys.version_info >= (2, 7)

# pylint: disable=import-error,invalid-name,undefined-variable,unused-import,wrong-import-position

if PY3: # pragma: nocover
    unicode_ = str
    int_ = int
    long_ = int
    unichr_ = chr
else: # pragma: nocover
    unicode_ = unicode
    int_ = int
    long_ = long
    unichr_ = unichr

if PY3: # pragma: nocover
    import configparser
else: # pragma: nocover
    import ConfigParser as configparser # pylint: disable=import-error

if PY3: # pragma: nocover
    StandardError_ = Exception
else: # pragma: nocover
    StandardError_ = StandardError

if PY33: # pragma: nocover
    from unittest import mock
else: # pragma: nocover
    import mock
