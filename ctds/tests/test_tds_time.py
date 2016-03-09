from datetime import datetime, time

import unittest

import ctds

class TestTdsTime(unittest.TestCase):

    def test_time(self):
        val = ctds.Time(0, 0, 0)
        self.assertEqual(val, time(0, 0, 0))

        val = ctds.Time(23, 59, 59)
        self.assertEqual(val, time(23, 59, 59))

    def test_fromticks(self):
        for case in (-1, 0, 123456789):
            self.assertEqual(
                ctds.TimeFromTicks(case),
                datetime.fromtimestamp(case).time()
            )

    def test_fromticks_typeerror(self):
        for case in (None, '', b''):
            self.assertRaises(TypeError, ctds.TimeFromTicks, case)

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
            self.assertRaises(TypeError, ctds.Time, *args)

    def test_valueerror(self):
        for args, part in (
                ((24, 0, 0), 'hour'),
                ((23, 60, 0), 'minute'),
                ((23, 59, 60), 'second'),
                ((-1, 0, 0), 'hour'),
                ((0, -1, 0), 'minute'),
                ((0, 0, -1), 'second'),
        ):
            try:
                ctds.Time(*args)
            except ValueError as ex:
                self.assertEqual(str(ex), '{0} is out of range'.format(part))
            else:
                self.fail('.Time() did not fail as expected') # pragma: nocover

