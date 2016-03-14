
# Workaround an exception at shutdown in multiprocessing by importing it here.
try:
    import multiprocessing
except:
    pass

import os
import setuptools
import setuptools.dist
import sys


CTDS_MAJOR_VERSION = 1
CTDS_MINOR_VERSION = 0
CTDS_PATCH_VERSION = 1

install_requires = [
]

tests_require = []
if (sys.version_info < (3, 3)):
    # Mock is part of the Python 3.3+ stdlib.
    tests_require.append('mock >= 0.7.2')

debug = os.environ.get('DEBUG')
extra_compile_args = ['-g', '-O0'] if debug else ['-O3'] # setuptools specifies -O2 -- override it
extra_link_args = ['-O1'] if debug else ['-O3'] # setuptools specifies -O2 -- override it

if os.environ.get('TDS_PROFILE'):
    extra_compile_args.append('-pg')
    extra_link_args.append('-pg')

if os.environ.get('TDS_COVER'):
    extra_compile_args += ['-fprofile-arcs', '-ftest-coverage']
    extra_link_args.append('-fprofile-arcs')

# pthread is required on OS X for thread-local storage support.
if sys.platform == 'darwin':
    extra_link_args.append('-lpthread')

setuptools.setup(
    name = 'ctds',
    version = '{0}.{1}.{2}'.format(
        CTDS_MAJOR_VERSION,
        CTDS_MINOR_VERSION,
        CTDS_PATCH_VERSION
    ),

    author = 'Joshua Lang',
    author_email = 'joshual@zillow.com',
    description = 'DB API 2.0-compliant interface based on freetds',
    keywords = 'freetds SQL DB',
    license = '(C) Zillow, Inc. 2015-',
    url = 'https://github.com/zillow/ctds',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Topic :: Utilities',
        'Programming Language :: Python :: 2.6',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3.3',
        'Programming Language :: Python :: 3.4',
        'Programming Language :: Python :: 3.5',
    ],

    packages = [
        'ctds'
    ],
    package_data = {
        'ctds': [
        ]
    },

    entry_points = {
        'console_scripts': [
        ]
    },

    ext_modules = [
        setuptools.Extension(
            '_tds',
            [
                os.path.join('ctds', file_)
                for file_ in (
                        'connection.c',
                        'cursor.c',
                        'parameter.c',
                        'pyutils.c',
                        'tds.c',
                        'type.c',
                )
            ],
            define_macros=[
                ('CTDS_MAJOR_VERSION', CTDS_MAJOR_VERSION),
                ('CTDS_MINOR_VERSION', CTDS_MINOR_VERSION),
                ('CTDS_PATCH_VERSION', CTDS_PATCH_VERSION),
            ],
            extra_compile_args=[
                '-DPY_SSIZE_T_CLEAN',
                '-DMSDBLIB',
                '-Wall',
                '-Wextra',
                '-Wconversion'
            ] + extra_compile_args,
            extra_link_args=[
                '-lsybdb'
            ] + extra_link_args,
        ),
    ],

    install_requires = install_requires,

    tests_require = tests_require,
    test_suite = 'ctds.tests',

    extras_require = {
        'tests': tests_require,
    },
)
