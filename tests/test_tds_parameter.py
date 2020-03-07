import re
import warnings

import ctds

from .base import TestExternalDatabase
from .compat import PY3, PY36, unicode_


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

    def test___repr__(self):
        for parameter, expected in (
                (
                    ctds.Parameter(b'123', output=True),
                    "ctds.Parameter(b'123', output=True)" if PY3 else "ctds.Parameter('123', output=True)"
                ),
                (
                    ctds.Parameter(unicode_('123'), output=False),
                    "ctds.Parameter('123')" if PY3 else "ctds.Parameter(u'123')"
                ),
                (
                    ctds.Parameter(None),
                    "ctds.Parameter(None)"
                ),
                (
                    ctds.Parameter(ctds.SqlVarBinary(b'4321', size=10)),
                    "ctds.Parameter(ctds.SqlVarBinary(b'4321', size=10))"
                    if PY3 else
                    "ctds.Parameter(ctds.SqlVarBinary('4321', size=10))"
                )
        ):
            self.assertEqual(repr(parameter), expected)

    def _test__cmp__(self, __cmp__, expected, oper):
        cases = (
            (ctds.Parameter(b'1234'), ctds.Parameter(b'123')),
            (ctds.Parameter(b'123'), ctds.Parameter(b'123')),
            (ctds.Parameter(b'123'), ctds.Parameter(b'123', output=True)),
            (ctds.Parameter(b'123'), ctds.Parameter(b'1234')),
            (ctds.Parameter(b'123'), b'123'),
            (ctds.Parameter(b'123'), ctds.Parameter(123)),
            (ctds.Parameter(b'123'), unicode_('123')),
            (ctds.Parameter(b'123'), ctds.SqlBinary(None)),
            (ctds.Parameter(b'123'), 123),
            (ctds.Parameter(b'123'), None),
        )

        for index, args in enumerate(cases):
            operation = '[{0}]: {1} {2} {3}'.format(index, repr(args[0]), oper, repr(args[1]))
            if expected[index] == TypeError:
                try:
                    __cmp__(*args)
                except TypeError as ex:
                    regex = (
                        r"'{0}' not supported between instances of '[^']+' and '[^']+'".format(oper)
                        if not PY3 or PY36
                        else
                        r'unorderable types: \S+ {0} \S+'.format(oper)
                    )
                    self.assertTrue(re.match(regex, str(ex)), ex)
                else:
                    self.fail('{0} did not fail as expected'.format(operation)) # pragma: nocover
            else:
                self.assertEqual(__cmp__(*args), expected[index], operation)

    def test___cmp__eq(self):
        self._test__cmp__(
            lambda left, right: left == right,
            (
                False,
                True,
                True,
                False,
                True,
                False,
                not PY3,
                False,
                False,
                False,
            ),
            '=='
        )

    def test___cmp__ne(self):
        self._test__cmp__(
            lambda left, right: left != right,
            (
                True,
                False,
                False,
                True,
                False,
                True,
                PY3,
                True,
                True,
                True,
            ),
            '!='
        )

    def test___cmp__lt(self):
        self._test__cmp__(
            lambda left, right: left < right,
            (
                False,
                False,
                False,
                True,
                False,
                TypeError if PY3 else False,
                TypeError if PY3 else False,
                TypeError if PY3 else False,
                TypeError if PY3 else False,
                TypeError if PY3 else False,
            ),
            '<'
        )

    def test___cmp__le(self):
        self._test__cmp__(
            lambda left, right: left <= right,
            (
                False,
                True,
                True,
                True,
                True,
                TypeError if PY3 else False,
                TypeError if PY3 else True,
                TypeError if PY3 else False,
                TypeError if PY3 else False,
                TypeError if PY3 else False,
            ),
            '<='
        )

    def test___cmp__gt(self):
        self._test__cmp__(
            lambda left, right: left > right,
            (
                True,
                False,
                False,
                False,
                False,
                TypeError if PY3 else True,
                TypeError if PY3 else False,
                TypeError if PY3 else True,
                TypeError if PY3 else True,
                TypeError if PY3 else True,
            ),
            '>'
        )

    def test___cmp__ge(self):
        self._test__cmp__(
            lambda left, right: left >= right,
            (
                True,
                True,
                True,
                False,
                True,
                TypeError if PY3 else True,
                TypeError if PY3 else True,
                TypeError if PY3 else True,
                TypeError if PY3 else True,
                TypeError if PY3 else True,
            ),
            '>='
        )

    def test_typeerror(self):
        for case in (None, object(), 123, 'foobar'):
            self.assertRaises(TypeError, ctds.Parameter, case, b'123')

        self.assertRaises(TypeError, ctds.Parameter)
        self.assertRaises(TypeError, ctds.Parameter, output=False)

        for case in (None, object(), 123, 'foobar'):
            self.assertRaises(TypeError, ctds.Parameter, b'123', output=case)

    def test_reuse(self):
        with self.connect() as connection:
            with connection.cursor() as cursor:
                for value in (
                        None,
                        123456,
                        unicode_('hello world'),
                        b'some bytes',
                ):
                    for output in (True, False):
                        parameter = ctds.Parameter(value, output=output)
                        for _ in range(0, 2):
                            # Ignore warnings generated due to output parameters
                            # used with result sets.
                            with warnings.catch_warnings(record=True):
                                cursor.execute(
                                    '''
                                    SELECT :0
                                    ''',
                                    (parameter,)
                                )
                                self.assertEqual(
                                    [tuple(row) for row in cursor.fetchall()],
                                    [(value,)]
                                )
