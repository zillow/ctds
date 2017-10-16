import datetime
import platform
import time

import unittest

import ctds

class TestTdsTimeStamp(unittest.TestCase):

    def test_timestamp(self):
        val = ctds.Timestamp(1, 1, 1, 0, 0, 0)
        self.assertEqual(val, datetime.datetime(1, 1, 1, 0, 0, 0))

        val = ctds.Timestamp(9999, 12, 31, 23, 59, 59)
        self.assertEqual(val, datetime.datetime(9999, 12, 31, 23, 59, 59))

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
                ctds.TimestampFromTicks(case),
                datetime.datetime.fromtimestamp(case)
            )

    def test_fromticks_typeerror(self):
        for case in (None, '', b''):
            self.assertRaises(TypeError, ctds.TimestampFromTicks, case)

    def test_typeerror(self):
        for args in (
                (),
                (1,),
                (1, 2),
                (1, 2, 3),
                (1, 2, 3, 4),
                (1, 2, 3, 4, 5),
                (None, None, None, None, None, None),
                ('', 2, 3, 4, 5, 6),
                (1, '', 3, 4, 5, 6),
                (1, 2, '', 4, 5, 6),
                (1, 2, 3, '', 5, 6),
                (1, 2, 3, 4, '', 6),
                (1, 2, 3, 3, 5, ''),
        ):
            self.assertRaises(TypeError, ctds.Timestamp, *args)

    def test_valueerror(self):
        for args, part in (
                ((0, 1, 1, 0, 0, 0), 'year'),
                ((1, 0, 1, 0, 0, 0), 'month'),
                ((1, 1, 0, 0, 0, 0), 'day'),
                ((10000, 1, 1, 0, 0, 0), 'year'),
                ((9999, 13, 1, 0, 0, 0), 'month'),
                ((9999, 12, 32, 0, 0, 0), 'day'),
                ((-1, 12, 1, 0, 0, 0), 'year'),
                ((1, -1, 1, 0, 0, 0), 'month'),
                ((1, 1, -1, 0, 0, 0), 'day'),
                ((1, 1, 1, 24, 0, 0), 'hour'),
                ((1, 1, 1, 23, 60, 0), 'minute'),
                ((1, 1, 1, 23, 59, 60), 'second'),
                ((1, 1, 1, -1, 0, 0), 'hour'),
                ((1, 1, 1, 0, -1, 0), 'minute'),
                ((1, 1, 1, 0, 0, -1), 'second'),
        ):
            try:
                ctds.Timestamp(*args)
            except ValueError as ex:
                self.assertEqual(str(ex), '{0} is out of range'.format(part))
            else:
                self.fail('.Timestamp() did not fail as expected') # pragma: nocover
