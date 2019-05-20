"""
********************************************************************************
* Name: setup.py
* Author: Gage Larsen, Andrew Clark
* Created On: April 18th, 2019
* Copyright: (c)
* License: BSD 2-Clause
********************************************************************************
"""
import os
from setuptools import setup, find_packages
from xms.core import __version__


# allow setup.py to be run from any path
os.chdir(os.path.normpath(os.path.join(os.path.abspath(__file__), os.pardir)))

requires = [
    'numpy', 'xmscore', 'xmsgrid'
]

version = __version__

setup(
    python_requires='==3.6.*',
    name='xmsinterp',
    version=version,
    packages=['xms.interp', 'xms.interp.interpolate'],
    include_package_data=True,
    license='BSD 2-Clause License',
    description='',
    author='Aquaveo',
    install_requires=requires,
    package_data={'': ['*.pyd', '*.so']},
    test_suite="tests",
)

