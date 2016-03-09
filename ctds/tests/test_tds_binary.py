import unittest

import ctds

class TestTdsBinary(unittest.TestCase):

    def test_binary(self):
        val = ctds.Binary(b'123')
        self.assertEqual(val.value, b'123')
        self.assertEqual(val.tdstype, ctds.BINARY)

        val = ctds.Binary(None)
        self.assertEqual(val.value, None)
        self.assertEqual(val.tdstype, ctds.BINARY)
