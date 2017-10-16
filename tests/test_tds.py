import re
import unittest
import pkg_resources

import ctds

from .compat import StandardError_

class TestTds(unittest.TestCase):

    def test_version(self):
        self.assertEqual(
            tuple(int(item) for item in ctds.__version__.split('.')),
            ctds.version_info
        )

        self.assertTrue(pkg_resources.get_distribution('ctds').version)

    def test_apilevel(self):
        self.assertEqual(ctds.apilevel, '2.0')

    def test_threadsafety(self):
        self.assertEqual(ctds.threadsafety, 1)

    def test_paramstyle(self):
        self.assertEqual(ctds.paramstyle, 'numeric')

    def test_freetds_version(self):
        version = ctds.freetds_version
        self.assertTrue(isinstance(version, str))
        self.assertTrue(
            re.match(
                r'^freetds v(?P<major>[\d]+)\.(?P<minor>[\d]+)(?:\.(?P<patch>[\d]+))?$',
                version
            )
        )

    def test_exceptions(self):
        self.assertTrue(issubclass(ctds.Warning, StandardError_))
        self.assertTrue(issubclass(ctds.Error, StandardError_))

        self.assertTrue(issubclass(ctds.InterfaceError, ctds.Error))
        self.assertTrue(issubclass(ctds.DatabaseError, ctds.Error))
        self.assertTrue(issubclass(ctds.DataError, ctds.DatabaseError))
        self.assertTrue(issubclass(ctds.OperationalError, ctds.DatabaseError))
        self.assertTrue(issubclass(ctds.IntegrityError, ctds.DatabaseError))
        self.assertTrue(issubclass(ctds.InternalError, ctds.DatabaseError))
        self.assertTrue(issubclass(ctds.ProgrammingError, ctds.DatabaseError))
        self.assertTrue(issubclass(ctds.NotSupportedError, ctds.DatabaseError))
