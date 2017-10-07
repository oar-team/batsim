#!/usr/bin/env python3
"""Check that batsim --version is consistent with batsim git releases."""

import argparse
import re
import subprocess
import sys


def version_from_string(input_string,
                        pattern='''^(v\d+\.\d+\.\d+).*$'''):
    """Retrieve a version string from a string."""
    r = re.compile(pattern)
    matches = r.match(input_string)

    if matches is None or len(matches.groups()) != 1:
        raise Exception("Cannot retrieve a version in '{}'".format(
            input_string))

    return matches.group(1)


def batsim_binary_version(batsim_command="batsim",
                          pattern='''(v\d+\.\d+\.\d+).*'''):
    """Retrieve the version from the batsim binary."""
    cmd = "{cmd} --version".format(cmd=batsim_command)
    p = subprocess.run(cmd, shell=True, check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    if p.returncode != 0:
        raise Exception("Command '{cmd}' returned {ret}.\n"
                        "Output:\n{o}\n\n"
                        "Error:\n{e}\n".format(cmd=cmd,
                                               ret=p.returncode,
                                               o=p.stdout.decode('utf-8'),
                                               e=p.stderr.decode('utf-8')))

    binary_version = p.stdout.decode('utf-8').strip()
    print("Batsim binary full version: {}".format(binary_version))
    return version_from_string(binary_version, pattern)


def batsim_git_version(
        git_command="git",
        git_path=".",
        git_subcommand='describe --match "v[0-9]*.[0-9]*.[0-9]*"',
        pattern='''(v\d+\.\d+\.\d+).*'''):
    """Retrieve the latest released Batsim version (in the git tags).

    This is done by calling git describe --match PATTERN,
    which should return either:
    - v1.2.0              if the current commit is a released version
    - v1.1.0-20-g0fcd0c9  if the current commit is after v1.1.0 in the tree
    - or fail if there is no released version before the current commit.
    """
    cmd = "{cmd} -C {p} {scmd}".format(cmd=git_command,
                                       p=git_path,
                                       scmd=git_subcommand)
    p = subprocess.run(cmd, shell=True, check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    if p.returncode != 0:
        raise Exception("Command '{cmd}' returned {ret}.\n"
                        "Output:\n{o}\n\n"
                        "Error:\n{e}\n".format(cmd=cmd,
                                               ret=p.returncode,
                                               o=p.stdout.decode('utf-8'),
                                               e=p.stderr.decode('utf-8')))

    git_version = p.stdout.decode('utf-8').strip()
    print("git full version: {}".format(git_version))
    return version_from_string(git_version, pattern)


def main():
    """Entry point."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--batsim-command',
                        type=str,
                        default='batsim',
                        help='The batsim command (without --version)')
    parser.add_argument('--batsim-git-dir',
                        type=str,
                        default='.',
                        help='The batsim git directory')

    args = parser.parse_args()

    bin_version = batsim_binary_version(batsim_command=args.batsim_command)
    git_version = batsim_git_version(git_path=args.batsim_git_dir)

    if bin_version == git_version:
        print("Batsim binary and git match on version {v}".format(
            v=bin_version))
    else:
        print("Version mismatch!\n"
              "- batsim binary: {bin}\n"
              "- git: {git}""".format(bin=bin_version, git=git_version))
        sys.exit(1)

if __name__ == '__main__':
    main()
