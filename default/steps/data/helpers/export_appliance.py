#!/usr/bin/env python2
# -*- coding: utf-8 -*-
"""Convert a disk image to many others formats with guestfish."""
from __future__ import division, unicode_literals

import os
# import time
import os.path as op
import sys
import subprocess
import argparse
import logging


logger = logging.getLogger(__name__)

tar_formats = ('tar', 'tar.gz', 'tgz', 'tar.bz2', 'tbz', 'tar.xz', 'txz',
               'tar.lzo', 'tzo')

disk_formats = ('qcow', 'qcow2', 'qed', 'vdi', 'raw', 'vmdk')


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


def tar_convert(disk, output, excludes, compression_level):
    """Convert image to a tar rootfs archive."""
    if compression_level in ("best", "fast"):
        compression_level_opt = "--%s" % compression_level
    else:
        compression_level_opt = "-%s" % compression_level

    compr = ""
    if output.endswith(('tar.gz', 'tgz')):
        try:
            compr = "| %s %s" % (which("pigz"), compression_level_opt)
        except:
            compr = "| %s %s" % (which("gzip"), compression_level_opt)
    elif output.endswith(('tar.bz2', 'tbz')):
        compr = "| %s %s" % (which("bzip2"), compression_level_opt)
    elif output.endswith(('tar.xz', 'txz')):
        compr = "| {} {} -c --threads=0 -".format(
            which("xz"), compression_level_opt)
    elif output.endswith(('tar.lzo', 'tzo')):
        compr = "| %s %s -c -" % (which("lzop"), compression_level_opt)

    tar_options_list = ["--selinux", "--acls", "--xattrs",
                        "--numeric-owner", "--one-file-system"] + \
                       ['--exclude="%s"' % s for s in excludes]
    tar_options = ' '.join(tar_options_list)
    directory = dir_path = os.path.dirname(os.path.realpath(disk))
    cmds = [
        which("mkdir") + " %s/.mnt" % directory,
        which("guestmount") + " --ro -i -a %s %s/.mnt" % (disk, directory),
        which("tar") + " -c %s -C %s/.mnt . %s > %s" % (tar_options, directory, compr, output),
        which("guestunmount") + " %s/.mnt" % directory,
        which("rmdir") + " %s/.mnt" % directory
        ]
    cmd = " && ".join(cmds)
    # NB: guestfish version >= 1.32 supports the special tar options, but not available in Debian stable (jessie): do not use for now
    #tar_options_list = ["selinux:true", "acls:true", "xattrs:true",
    #                    "numericowner:true",
    #                    "excludes:\"%s\"" % ' '.join(excludes)]
    #tar_options = ' '.join(tar_options_list)
    #cmd = which("guestfish") + \
    #    " --ro -i tar-out -a %s / - %s %s > %s"
    #cmd = cmd % (disk, tar_options, compr, output)

    proc = subprocess.Popen(cmd, env=os.environ.copy(), shell=True)
    proc.communicate()
    if proc.returncode:
        raise subprocess.CalledProcessError(proc.returncode, cmd)


def qemu_convert(disk, output_fmt, output_filename):
    """Convert the disk image filename to disk image output_filename."""
    binary = which("qemu-img")
    cmd = [binary, "convert", "-O", output_fmt, disk, output_filename]
    if output_fmt in ("qcow", "qcow2"):
        cmd.insert(2, "-c")
    proc = subprocess.Popen(cmd, env=os.environ.copy(), shell=False)
    proc.communicate()
    if proc.returncode:
        raise subprocess.CalledProcessError(proc.returncode, ' '.join(cmd))


def run_guestfish_script(disk, script, mount=""):
    """
    Run guestfish script.
    Mount should be in ("read_only", "read_write", "ro", "rw")
    """
    args = [which("guestfish"), '-a', disk]
    if mount in ("read_only", "read_write", "ro", "rw"):
        args.append('-i')
        if mount in mount in ("read_only", "ro"):
            args.append('--ro')
        else:
            args.append('--rw')
    else:
        script = "run\n%s" % script
    proc = subprocess.Popen(args,
                            stdin=subprocess.PIPE,
                            env=os.environ.copy())
    proc.communicate(input=script.encode('utf-8'))
    if proc.returncode:
        raise subprocess.CalledProcessError(proc.returncode, ' '.join(args))


def guestfish_zerofree(filename):
    """Fill free space with zero"""
    logger.info(guestfish_zerofree.__doc__)
    cmd = "virt-filesystems -a %s" % filename
    fs = subprocess.check_output(cmd.encode('utf-8'),
                                 stderr=subprocess.STDOUT,
                                 shell=True,
                                 env=os.environ.copy())
    list_fs = fs.decode('utf-8').split()
    logger.info('\n'.join(('  `--> %s' % i for i in list_fs)))
    script = '\n'.join(('zerofree %s' % i for i in list_fs))
    run_guestfish_script(filename, script, mount="read_only")


def convert_disk_image(args):
    """Convert disk to another format."""
    filename = op.abspath(args.file.name)
    output = op.abspath(args.output)

    os.environ['LIBGUESTFS_CACHEDIR'] = os.getcwd()
    if args.verbose:
        os.environ['LIBGUESTFS_DEBUG'] = '1'

    # sometimes guestfish fails because of other virtualization tools are
    # still running use a test and retry to wait for availability
    # attempts = 0
    # while attempts < 3:
    #    try:
    #        logger.info("Waiting for virtualisation to be available...")
    #        run_guestfish_script(filename, "cat /etc/hostname", mount='ro')
    #        break
    #    except:
    #        attempts += 1
    #        time.sleep(1)

    if args.zerofree:
        guestfish_zerofree(filename)

    for fmt in args.formats:
        if fmt in (tar_formats + disk_formats):
            output_filename = "%s.%s" % (output, fmt)
            if output_filename == filename:
                continue
            logger.info("Creating %s" % output_filename)
            try:
                if fmt in tar_formats:
                    tar_convert(filename, output_filename,
                                args.tar_excludes,
                                args.tar_compression_level)
                else:
                    qemu_convert(filename, fmt, output_filename)
            except ValueError as exp:
                logger.error("Error: %s" % exp)


if __name__ == '__main__':
    allowed_formats = tar_formats + disk_formats
    allowed_formats_help = 'Allowed values are ' + ', '.join(allowed_formats)

    allowed_levels = ["%d" % i for i in range(1, 10)] + ["best", "fast"]
    allowed_levels_helps = 'Allowed values are ' + ', '.join(allowed_levels)

    parser = argparse.ArgumentParser(
        description=sys.modules[__name__].__doc__,
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument('file', action="store", type=argparse.FileType('r'),
                        help='Disk image filename')
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
    parser.add_argument('--zerofree', action="store_true", default=False,
                        help='Zero free unallocated blocks from ext2/3 '
                             'file-systems before export to reduce image size')
    parser.add_argument('--verbose', action="store_true", default=False,
                        help='Enable very verbose messages')
    log_format = '%(levelname)s: %(message)s'
    level = logging.INFO
    args = parser.parse_args()
    if args.verbose:
        level = logging.DEBUG

    handler = logging.StreamHandler(sys.stdout)
    handler.setLevel(level)
    handler.setFormatter(logging.Formatter(log_format))

    logger.setLevel(level)
    logger.addHandler(handler)

    convert_disk_image(args)
