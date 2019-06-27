import threading
import time
import unittest
import warnings

from ctds.pool import ConnectionPool

from ..compat import mock


class TestConnectionPool(unittest.TestCase):

    maxDiff = None

    def test_acquire(self):
        mock_dbapi2 = mock.MagicMock()

        params = {
            'server': 'hostname',
            'arg1': 1234,
            'arg2': 'foobar',
        }
        pool = ConnectionPool(mock_dbapi2, params, maxsize=10)

        conn = pool.acquire()

        mock_dbapi2.assert_has_calls(
            [
                mock.call.connect(**params),
            ]
        )
        pool.release(conn)

        mock_dbapi2.assert_has_calls(
            [
                mock.call.connect(**params),
                mock.call.connect().rollback(),
            ]
        )

        mock_dbapi2.reset_mock()

        conn2 = pool.acquire()

        self.assertEqual(id(conn), id(conn2))

        pool.release(conn2)

        mock_dbapi2.assert_has_calls(
            [
                mock.call.connect().rollback(),
            ]
        )

    def test_release_exception(self): # pylint: disable=no-self-use
        class MockDBAPI2Error(Exception):
            pass

        mock_dbapi2 = mock.MagicMock()
        type(mock_dbapi2).Error = \
            mock.PropertyMock(return_value=MockDBAPI2Error)

        pool = ConnectionPool(mock_dbapi2, {}, maxsize=10)
        mock_dbapi2.connect.return_value.rollback.side_effect = MockDBAPI2Error

        pool.release(pool.acquire())

        # Verify the connection is closed on rollback error.
        mock_dbapi2.assert_has_calls(
            [
                mock.call.connect(),
                mock.call.connect().rollback(),
                mock.call.connect().close(),
            ]
        )

        # ... and the connection is not returned to the pool.
        mock_dbapi2.connect.return_value.rollback.side_effect = None

        mock_dbapi2.reset_mock()
        pool.release(pool.acquire())

        mock_dbapi2.assert_has_calls(
            [
                mock.call.connect(),
                mock.call.connect().rollback(),
            ]
        )

    def test_contextmanager(self):
        mock_dbapi2 = mock.MagicMock()
        pool = ConnectionPool(mock_dbapi2, {}, maxsize=10)

        with pool.connection() as connection:
            mock_dbapi2.assert_has_calls(
                [
                    mock.call.connect()
                ]
            )
            self.assertEqual(
                id(connection),
                id(mock_dbapi2.connect.return_value)
            )

        mock_dbapi2.assert_has_calls(
            [
                mock.call.connect(),
                mock.call.connect().rollback(),
            ]
        )

    def test_contextmanager_error(self): # pylint: disable=no-self-use
        class MockDBAPI2Error(Exception):
            pass

        mock_dbapi2 = mock.MagicMock()
        pool = ConnectionPool(mock_dbapi2, {}, maxsize=10)

        try:
            with pool.connection():
                raise MockDBAPI2Error
        except MockDBAPI2Error:
            pass

        mock_dbapi2.assert_has_calls(
            [
                mock.call.connect(),
                mock.call.connect().rollback(),
            ]
        )

    def test_finalize(self): # pylint: disable=no-self-use
        mock_dbapi2 = mock.MagicMock()
        pool = ConnectionPool(mock_dbapi2, {}, maxsize=10)

        pool.release(pool.acquire())

        pool.finalize()

        mock_dbapi2.assert_has_calls(
            [
                mock.call.connect(),
                mock.call.connect().rollback(),
                mock.call.connect().close(),
            ]
        )

    def test_mismatched_release(self):
        mock_dbapi2 = mock.MagicMock()
        pool = ConnectionPool(mock_dbapi2, {}, maxsize=10)
        pool.acquire()

        with warnings.catch_warnings(record=True) as warns:
            pool.finalize()

        self.assertEqual(len(warns), 1)
        self.assertEqual(
            [str(warn.message) for warn in warns],
            ['finalize() called with unreleased connections'] * len(warns)
        )

    def test_idlettl(self): # pylint: disable=no-self-use
        idlettl = 10

        mock_dbapi2 = mock.MagicMock()
        pool = ConnectionPool(mock_dbapi2, {}, idlettl=idlettl)

        now = time.time()

        with mock.patch('time.time', return_value=now) as mock_time:
            conn1 = pool.acquire()
            pool.release(conn1)

            mock_time.return_value += idlettl

            conn2 = pool.acquire()
            pool.release(conn2)

            mock_time.return_value += idlettl + 1

            conn3 = pool.acquire()
            pool.release(conn3)

            mock_dbapi2.assert_has_calls(
                [
                    mock.call.connect(),
                    mock.call.connect().rollback(), # .release(conn1)
                    mock.call.connect().rollback(), # .release(conn2)
                    mock.call.connect().close(), # .release(conn2)
                    mock.call.connect(),
                    mock.call.connect().rollback(), # .release(conn3)
                ]
            )

    def test_maxsize(self): # pylint: disable=no-self-use
        mock_dbapi2 = mock.MagicMock()
        pool = ConnectionPool(mock_dbapi2, {}, maxsize=1)

        conn1 = pool.acquire()
        pool.release(conn1)
        conn2 = pool.acquire()
        mock_dbapi2.assert_has_calls(
            [
                mock.call.connect(),
                mock.call.connect().rollback(), # .release(conn1)
            ]
        )

        mock_dbapi2.reset_mock()
        pool.release(conn2)

        mock_dbapi2.assert_has_calls(
            [
                mock.call.connect().rollback(), # .release(conn2)
            ]
        )

        mock_dbapi2.reset_mock()
        conn3 = pool.acquire() # this should come from the pool
        mock_dbapi2.assert_has_calls([])

        mock_dbapi2.reset_mock()
        conn4 = pool.acquire() # new connection
        mock_dbapi2.assert_has_calls(
            [
                mock.call.connect(),
            ]
        )

        mock_dbapi2.reset_mock()
        pool.release(conn4) # return conn4 to pool
        mock_dbapi2.assert_has_calls(
            [
                mock.call.connect().rollback(),
            ]
        )

        mock_dbapi2.reset_mock()
        pool.release(conn3) # pool is full, close the connection
        mock_dbapi2.assert_has_calls(
            [
                mock.call.connect().rollback(),
                mock.call.connect().close(),
            ]
        )

    def test_block(self): # pylint: disable=no-self-use
        mock_dbapi2 = mock.MagicMock()
        pool = ConnectionPool(mock_dbapi2, {}, maxsize=1, block=True)

        conn1 = pool.acquire()

        def target():
            time.sleep(.01)
            pool.release(conn1)

        thread = threading.Thread(target=target)
        thread.start()
        conn2 = pool.acquire() # should block waiting for background thread to relase conn1
        thread.join()
        pool.release(conn2)

        mock_dbapi2.assert_has_calls(
            [
                mock.call.connect(),
                mock.call.connect().rollback(),
                mock.call.connect().rollback(),
            ]
        )
