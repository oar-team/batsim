#!/usr/bin/env python2
# -*- coding: utf-8 -*-
"""Check if a bootloader need to be installed."""
from __future__ import division, unicode_literals

import sys
import argparse

disk_formats = ('qcow', 'qcow2', 'qed', 'vdi', 'raw', 'vmdk')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description=sys.modules[__name__].__doc__,
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument('-F', '--formats', action="store", type=str, nargs='+',
                        help='Output formats.', metavar='fmt', required=True)
    try:
        args = parser.parse_args()
        rv = "no"
        for fmt in args.formats:
            if fmt in disk_formats:
                rv = "yes"
        print(rv)
    except Exception as exc:
        sys.stderr.write(u"\nError: %s\n" % exc)
        sys.exit(1)
