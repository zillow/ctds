from contextlib import contextmanager
from collections import namedtuple
import threading
import time
import warnings

PooledConnection = namedtuple('PooledConnection', ['connection', 'released'])


class ConnectionPool(object): # pylint: disable=too-many-instance-attributes
    '''
    A basic connection pool for DB-API 2.0-compliant database drivers.

    Example usage:

    .. code-block:: python

        config = {
            'server': 'my-host',
            'database': 'MyDefaultDatabase',
            'user': 'my-username',
            'password': 'my-password',
            'appname': 'my-app-name',
            'timeout': 5
        }
        pool = ConnectionPool(pydbmod, config)

        conn = pool.acquire()
        try:
            cursor = conn.cursor()
            try:
                cursor.execute('SELECT @@VERSION')
            finally:
                cursor.close()
        finally:
            pool.release(conn)


    .. versionadded:: 1.2

    .. note:: Idle connection collection only occurs during calls to
        :py:meth:`.acquire()` and does not happen automatically in the
        background.

    :param dbapi2: The DB-API 2 module used to create connections.
        This module must implement the :pep:`0249` specification.

    :param dict params: Connection parameters to pass directly to the
        :pep:`0249#connect` method when creating a new connection.

    :param float idlettl: The maximum time, in seconds, a connection can sit
        idle before it is discarded. If not specified, connections are retained
        indefinitely.

    :param int maxsize: The maximum number of connections to keep in
        the pool.

    :param bool block: Block until a connection is released back to the
        pool? This is only applicable when a `maxsize` value is specified. If
        `False`, a new connection will be created as needed, but the number of
        connections retained in the pool will never exceed `maxsize`.
    '''

    # Use __slots__ to minimize footprint.
    __slots__ = (
        '_block',
        '_connection_args',
        '_condition',
        '_dbapi2',
        '_idlettl',
        '_maxsize',
        '_nconnections',
        '_pool',
    )

    def __init__( # pylint: disable=too-many-arguments
            self,
            dbapi2,
            params,
            idlettl=None,
            maxsize=None,
            block=False,
    ):
        self._dbapi2 = dbapi2
        self._connection_args = params

        # The number of connections currently open.
        self._nconnections = 0

        self._condition = threading.Condition()

        self._maxsize = maxsize
        self._block = block
        self._idlettl = idlettl

        # Pool of connections. Ordered from least to most recently used.
        self._pool = []

    def __del__(self):
        self.finalize()

    @contextmanager
    def connection(self):
        '''
        Acquire a connection in a managed context for use with the Python
        `with` keyword.

        .. code-block:: python

            with pool.connection() as connection:
                # do stuff with connection

        '''
        connection = self.acquire()
        yield connection
        self.release(connection)

    def acquire(self):
        '''
        Get a new connection from the pool.
        This will return an existing connection, if one is available in the
        pool, or create a new connection.

        .. warning:: If the pool was created with `maxsize` and `block=True`,
            this method may block until a connection is available in the pool.
        '''

        self._condition.acquire()
        try:
            # Wait for a connection if there is an upper bound to the pool.
            if self._maxsize is not None and self._block:
                while not self._pool and self._nconnections == self._maxsize:
                    self._condition.wait(timeout=None) # block indefinitely

            # Check the pool for a non-stale connection.
            while len(self._pool) > 0:
                pooledconn = self._pool.pop(0) # get least recently used connection
                if self._idlettl is not None and (pooledconn.released + self._idlettl) < time.time():
                    pooledconn.connection.close()
                    self._nconnections -= 1
                else:
                    return pooledconn.connection

            connection = self._dbapi2.connect(*(), **self._connection_args.copy())
            self._nconnections += 1

            return connection
        finally:
            self._condition.release()

    def release(self, connection):
        '''
        Return a connection back to the pool.

        Prior to release, :py:meth:`ctds.Connection.rollback()` is called to
        rollback any pending transaction.

        .. note:: This must be called once for every successful call to
            :py:meth:`.acquire()`.

        :param connection: The connection object returned by
            :py:meth:`.acquire()`.
        '''
        try:
            # Rollback the existing connection, closing on failure.
            connection.rollback()
        except self._dbapi2.Error:
            self._close(connection)
            return

        self._condition.acquire()
        try:
            if self._maxsize is None or self._maxsize > len(self._pool):
                self._pool.append(PooledConnection(connection, time.time()))
                self._condition.notify()
            else:
                self._close(connection)
        finally:
            self._condition.release()

    def finalize(self):
        '''
        Release all connections contained in the pool.

        .. note:: This should be called to cleanly shutdown the pool, i.e.
            on process exit.
        '''
        self._condition.acquire()
        try:
            if self._nconnections != len(self._pool):
                warnings.warn('finalize() called with unreleased connections', RuntimeWarning, 2)

            while self._pool:
                self._close(self._pool.pop().connection)
            self._nconnections = 0
        finally:
            self._condition.release()

    def _close(self, connection):
        assert self._nconnections > 0, \
            '.release() called multiple times for same connection'
        connection.close()
        self._nconnections -= 1
