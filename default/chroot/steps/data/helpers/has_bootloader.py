#!/usr/bin/env python2
# -*- coding: utf-8 -*-
"""Check if a disk image have a bootloader."""
import os
import os.path as op
import sys
import subprocess
import argparse
import logging
import mimetypes


logger = logging.getLogger(__name__)


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


def check_bootloader(disk):
    """Run guestfish script."""
    args = [which("guestfish"), '-a', disk]
    os.environ['LIBGUESTFS_CACHEDIR'] = os.getcwd()
    proc = subprocess.Popen(args,
                            stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            env=os.environ.copy())

    cmd = "file /dev/sda"
    stdout, stderr = proc.communicate(input=("run\n%s" % cmd).encode('utf-8'))
    if proc.returncode:
        return False

    output = ("%s\n%s" % (stdout.decode('utf-8').lower().strip(),
                          stderr.decode('utf-8').lower().strip())).strip()

    logger.debug("%s => %s" % (cmd, output.strip()))
    for word in ("mbr", "bootloader", "boot sector"):
        if word in output:
            return True


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description=sys.modules[__name__].__doc__,
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument('file', action="store", type=argparse.FileType('r'),
                        help='Disk image filename')
    parser.add_argument('--verbose', action="store_true", default=False,
                        help='Enable very verbose messages')
    args = parser.parse_args()

    handler = logging.StreamHandler()
    if args.verbose:
        logger.setLevel(logging.DEBUG)
        handler.setLevel(logging.DEBUG)
    else:
        logger.setLevel(logging.INFO)
        handler.setLevel(logging.INFO)
    handler.setFormatter(logging.Formatter('%(levelname)s: %(message)s'))
    logger.addHandler(handler)

    try:
        args = parser.parse_args()
        filename = op.abspath(args.file.name)
        file_type, _ = mimetypes.guess_type(filename)
        if file_type is None:
            if check_bootloader(filename):
                logger.info("Bootloader is detected")
                sys.exit(0)
        logger.error("Bootloader is missing")
        sys.exit(1)
    except Exception as exc:
        sys.stderr.write(u"\nError: %s\n" % exc)
        sys.exit(1)
