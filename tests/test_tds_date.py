import datetime
import platform
import time

import unittest

import ctds

class TestTdsDate(unittest.TestCase):

    def test_date(self):
        val = ctds.Date(1, 2, 3)
        self.assertEqual(val, datetime.date(1, 2, 3))

        val = ctds.Date(9999, 12, 31)
        self.assertEqual(val, datetime.date(9999, 12, 31))

    def test_fromticks(self):
        cases = [
            86400,
            123456789,
            time.time()
        ]
        if platform.system() != 'Windows': # pragma: nobranch
            cases += [-1, 0]

        for case in cases:
            self.assertEqual(
                ctds.DateFromTicks(case),
                datetime.datetime.fromtimestamp(case).date()
            )

    def test_fromticks_typeerror(self):
        for case in (None, '', b''):
            self.assertRaises(TypeError, ctds.DateFromTicks, case)

    def test_typeerror(self):
        for args in (
                (),
                (1,),
                (1, 2),
                (None, None, None),
                ('', 1, 2),
                (1, '', 2),
                (1, 1, ''),
        ):
            self.assertRaises(TypeError, ctds.Date, *args)

    def test_valueerror(self):
        for args, part in (
                ((0, 1, 1), 'year'),
                ((1, 0, 1), 'month'),
                ((1, 1, 0), 'day'),
                ((10000, 1, 1), 'year'),
                ((9999, 13, 1), 'month'),
                ((9999, 12, 32), 'day'),
                ((-1, 12, 1), 'year'),
                ((1, -1, 1), 'month'),
                ((1, 1, -1), 'day'),
        ):
            try:
                ctds.Date(*args)
            except ValueError as ex:
                self.assertEqual(str(ex), '{0} is out of range'.format(part))
            else:
                self.fail('.Date() did not fail as expected') # pragma: nocover
