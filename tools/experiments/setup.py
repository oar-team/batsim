#!/usr/bin/env python
# -*- coding: utf-8 -*-
import re
import os.path as op


from codecs import open
from setuptools import setup, find_packages


def read(fname):
    ''' Return the file content. '''
    here = op.abspath(op.dirname(__file__))
    with open(op.join(here, fname), 'r', 'utf-8') as fd:
        return fd.read()

readme = read('README.rst')
changelog = read('CHANGES.rst').replace('.. :changelog:', '')

requirements = [
    'pandas',
    'matplotlib',
]

# truc krado qui va chercher la version
version = ''
version = re.search(r'^__version__\s*=\s*[\'"]([^\'"]*)[\'"]',
                    read(op.join('evalys', '__init__.py')),
                    re.MULTILINE).group(1)

if not version:
    raise RuntimeError('Cannot find version information')


setup(
    name='evalys',
    author="Olivier Richard",
    author_email='olivier.richard@imag.fr',
    version=version,
    url='https://github.com/oar-team/evalys',
    packages=find_packages(),
    install_requires=requirements,
    include_package_data=True,
    zip_safe=False,
    description="Infrastructure Performance Evaluation Toolkit",
    long_description=readme + '\n\n' + changelog,
    keywords='evalys',
    license='BSD',
    classifiers=[
        'Development Status :: 4 - Beta',
        'License :: OSI Approved :: BSD License',
        'Programming Language :: Python',
        'Programming Language :: Python :: 3',
        'Topic :: System :: Clustering',
    ],
    entry_points={
        'console_scripts': [
            'evalys=evalys.evalys:main',
        ],
    }
)
