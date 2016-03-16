import ctds

from .base import TestExternalDatabase

class TestTdsParameter(TestExternalDatabase):

    def test___doc__(self):
        self.assertEqual(
            ctds.Parameter.__doc__,
            '''\
Parameter(value, output=False)

Explicitly define a parameter for :py:meth:`.callproc`,
:py:meth:`.execute`, or :py:meth:`.executemany`. This is necessary
to indicate whether a parameter is *SQL* `OUTPUT` or `INPUT/OUTPUT`
parameter.

:param object value: The parameter's value.
:param bool output: Is the parameter an output parameter.
'''
        )

    def test_parameter(self):
        param1 = ctds.Parameter(b'123', output=True)
        self.assertEqual(param1.value, b'123')
        self.assertTrue(isinstance(param1, ctds.Parameter))

        param2 = ctds.Parameter(b'123')
        self.assertEqual(param1.value, b'123')
        self.assertEqual(type(param1), type(param2))
        self.assertTrue(isinstance(param2, ctds.Parameter))

    def test_typeerror(self):
        for case in (None, object(), 123, 'foobar'):
            self.assertRaises(TypeError, ctds.Parameter, case, b'123')

        self.assertRaises(TypeError, ctds.Parameter)
        self.assertRaises(TypeError, ctds.Parameter, output=False)

        for case in (None, object(), 123, 'foobar'):
            self.assertRaises(TypeError, ctds.Parameter, b'123', output=case)
