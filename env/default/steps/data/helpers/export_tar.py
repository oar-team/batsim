#!/usr/bin/env python2
# -*- coding: utf-8 -*-
"""Convert a rootfs directory to tar archives."""
from __future__ import division, unicode_literals

import os
import os.path as op
import sys
import subprocess
import argparse
import logging


logger = logging.getLogger(__name__)

tar_formats = ('tar', 'tar.gz', 'tgz', 'tar.bz2', 'tbz', 'tar.xz', 'txz',
               'tar.lzo', 'tzo')


def which(command):
    """Locate a command.
    Snippet from: http://stackoverflow.com/a/377028
    """
    def is_exe(fpath):
        return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

    fpath, fname = os.path.split(command)
    if fpath:
        if is_exe(command):
            return command
    else:
        for path in os.environ["PATH"].split(os.pathsep):
            path = path.strip('"')
            exe_file = os.path.join(path, command)
            if is_exe(exe_file):
                return exe_file

    raise ValueError("Command '%s' not found" % command)


def tar_convert(directory, output, excludes, compression_level):
    """Convert image to a tar rootfs archive."""
    if compression_level in ("best", "fast"):
        compression_level_opt = "--%s" % compression_level
    else:
        compression_level_opt = "-%s" % compression_level

    compr = ""
    if output.endswith(('tar.gz', 'tgz')):
        compr = "| %s %s" % (which("gzip"), compression_level_opt)
    elif output.endswith(('tar.bz2', 'tbz')):
        compr = "| %s %s" % (which("bzip2"), compression_level_opt)
    elif output.endswith(('tar.xz', 'txz')):
        compr = "| %s %s -c -" % (which("xz"), compression_level_opt)
    elif output.endswith(('tar.lzo', 'tzo')):
        compr = "| %s %s -c -" % (which("lzop"), compression_level_opt)

    tar_options_list = ["--numeric-owner", "--one-file-system"
                        ' '.join(('--exclude="%s"' % s for s in excludes))]
    tar_options = ' '.join(tar_options_list)
    cmd = which("tar") + " -cf - %s -C %s $(cd %s; ls -A) %s > %s"
    cmd = cmd % (tar_options, directory, directory, compr, output)
    logger.debug(cmd)
    proc = subprocess.Popen(cmd, env=os.environ.copy(), shell=True)
    proc.communicate()
    if proc.returncode:
        raise subprocess.CalledProcessError(proc.returncode, cmd)


def export(args):
    """Convert disk to another format."""
    filename = op.abspath(args.input)
    output = op.abspath(args.output)

    for fmt in args.formats:
        if fmt in tar_formats:
            output_filename = "%s.%s" % (output, fmt)
            logger.info("Creating %s" % output_filename)
            if output_filename == filename:
                logger.error("Please give a different output filename.")
            else:
                try:
                    tar_convert(filename, output_filename,
                                args.tar_excludes,
                                args.tar_compression_level)
                except ValueError as exp:
                    logger.error("Error: %s" % exp)


if __name__ == '__main__':
    allowed_formats = tar_formats
    allowed_formats_help = 'Allowed values are ' + ', '.join(allowed_formats)

    allowed_levels = ["%d" % i for i in range(1, 10)] + ["best", "fast"]
    allowed_levels_helps = 'Allowed values are ' + ', '.join(allowed_levels)

    parser = argparse.ArgumentParser(
        description=sys.modules[__name__].__doc__,
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument('input', action="store",
                        help='input')
    parser.add_argument('-F', '--formats', action="store", type=str, nargs='+',
                        help='Output format. ' + allowed_formats_help,
                        choices=allowed_formats, metavar='fmt', required=True)
    parser.add_argument('-o', '--output', action="store", type=str,
                        help='Output filename (without file extension)',
                        required=True, metavar='filename')
    parser.add_argument('--tar-compression-level', action="store", type=str,
                        default="9", choices=allowed_levels, metavar='lvl',
                        help="Compression level. " + allowed_levels_helps)
    parser.add_argument('--tar-excludes', action="store", type=str, nargs='+',
                        help="Files to excluded from archive",
                        metavar='pattern', default=[])
    parser.add_argument('--verbose', action="store_true", default=False,
                        help='Enable very verbose messages')
    log_format = '%(levelname)s: %(message)s'
    level = logging.INFO
    try:
        args = parser.parse_args()
        if args.verbose:
            level = logging.DEBUG

        handler = logging.StreamHandler(sys.stdout)
        handler.setLevel(level)
        handler.setFormatter(logging.Formatter(log_format))

        logger.setLevel(level)
        logger.addHandler(handler)
        export(args)
    except Exception as exc:
        sys.stderr.write(u"\nError: %s\n" % exc)
        sys.exit(1)
