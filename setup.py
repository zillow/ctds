#!/usr/bin/env python

import glob
import io
import os
import os.path
import platform
import sys

import setuptools
import setuptools.dist

# Version information is defined here and compiled into the extension.
CTDS_MAJOR_VERSION = 1
CTDS_MINOR_VERSION = 11
CTDS_PATCH_VERSION = 0


TESTS_REQUIRE = []
if sys.version_info < (3, 3):
    # Mock is part of the Python 3.3+ stdlib.
    TESTS_REQUIRE.append('mock >= 0.7.2')

STRICT = os.environ.get('CTDS_STRICT')
WINDOWS = platform.system() == 'Windows'
COVERAGE = os.environ.get('CTDS_COVER', False)

LIBRARIES = [
    'sybdb',
    'ct', # required for ct_config only
]

EXTRA_COMPILE_ARGS = []
EXTRA_LINK_ARGS = []


if not WINDOWS:
    EXTRA_COMPILE_ARGS = [
    ]
    if STRICT:
        EXTRA_COMPILE_ARGS += [
            '-ansi',
            '-Wall',
            '-Wextra',
            '-Werror',
            '-Wconversion',
            '-Wpedantic',
            '-std=c99',
        ]

    if os.environ.get('CTDS_PROFILE'):
        EXTRA_COMPILE_ARGS.append('-pg')
        if os.path.isdir(os.environ['CTDS_PROFILE']):
            EXTRA_COMPILE_ARGS += ['-fprofile-dir', '"{0}"'.format(os.environ['CTDS_PROFILE'])]
        EXTRA_LINK_ARGS.append('-pg')

    if COVERAGE:
        EXTRA_COMPILE_ARGS += ['-fprofile-arcs', '-ftest-coverage']
        EXTRA_LINK_ARGS.append('-fprofile-arcs')

    # pthread is required on OS X for thread-local storage support.
    if sys.platform == 'darwin':
        EXTRA_LINK_ARGS.append('-lpthread')
else:
    if STRICT:
        EXTRA_COMPILE_ARGS += [
            '/WX',
            '/w14242',
            '/w14254',
            '/w14263',
            '/w14265',
            '/w14287',
            '/we4289',
            '/w14296',
            '/w14311',
            '/w14545',
            '/w14546',
            '/w14547',
            '/w14549',
            '/w14555',
            '/w14619',
            '/w14640',
            '/w14826',
            '/w14905',
            '/w14906',
            '/w14928',
            '/Zi'
        ]
    if COVERAGE:
        EXTRA_COMPILE_ARGS.append('/Od')

        # Generate a PDB for code coverage.
        EXTRA_LINK_ARGS.append('/DEBUG')
    LIBRARIES += [
        'shell32',
        'ws2_32'
    ]


def splitdirs(name):
    dirs = os.environ.get(name)
    return dirs.split(os.pathsep) if dirs else []


def read(*names, **kwargs):
    with io.open(
            os.path.join(os.path.dirname(__file__), *names),
            encoding=kwargs.get('encoding', 'utf-8')
    ) as file_:
        return file_.read()


setuptools.setup(
    name='ctds',
    version='{0}.{1}.{2}'.format(
        CTDS_MAJOR_VERSION,
        CTDS_MINOR_VERSION,
        CTDS_PATCH_VERSION
    ),

    author='Joshua Lang',
    author_email='joshual@zillow.com',
    description='DB API 2.0-compliant driver for SQL Server',
    long_description=read('README.rst'),
    long_description_content_type='text/x-rst',
    keywords=[
        'freetds',
        'mssql',
        'SQL',
        'T-SQL',
        'TDS',
        'SQL Server',
        'DB-API',
        'PEP-0249',
        'database',
    ],
    license='MIT',
    url='https://github.com/zillow/ctds',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Operating System :: MacOS :: MacOS X',
        'Operating System :: POSIX :: Linux',
        'Operating System :: Microsoft :: Windows',
        'Programming Language :: C',
        'Programming Language :: Python :: 2.6',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.3',
        'Programming Language :: Python :: 3.4',
        'Programming Language :: Python :: 3.5',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: Implementation :: CPython',
        'Programming Language :: SQL',
        'Topic :: Database',
    ],

    python_requires='>=2.6, !=3.0.*, !=3.1.*, !=3.2.*',

    packages=setuptools.find_packages('src'),
    package_data={
        'ctds': []
    },
    package_dir={'': 'src'},

    entry_points={
        'console_scripts': []
    },

    ext_modules=[
        setuptools.Extension(
            '_tds',
            glob.glob(os.path.join('src', 'ctds', '*.c')),
            define_macros=[
                ('CTDS_MAJOR_VERSION', CTDS_MAJOR_VERSION),
                ('CTDS_MINOR_VERSION', CTDS_MINOR_VERSION),
                ('CTDS_PATCH_VERSION', CTDS_PATCH_VERSION),
                ('PY_SSIZE_T_CLEAN', '1'),
                ('MSDBLIB', '1'),
            ],
            include_dirs=splitdirs('CTDS_INCLUDE_DIRS'),
            library_dirs=splitdirs('CTDS_LIBRARY_DIRS'),
            # runtime_library_dirs is not supported on Windows.
            runtime_library_dirs=[] if WINDOWS else splitdirs('CTDS_RUNTIME_LIBRARY_DIRS'),
            extra_compile_args=EXTRA_COMPILE_ARGS,
            extra_link_args=EXTRA_LINK_ARGS,
            libraries=LIBRARIES,
            language='c',
        ),
    ],

    install_requires=[],

    tests_require=TESTS_REQUIRE,
    test_suite='tests',

    extras_require={
        'tests': TESTS_REQUIRE,
    },

    # Prevent easy_install from warning about use of __file__.
    zip_safe=False
)
