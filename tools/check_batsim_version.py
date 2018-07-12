#!/usr/bin/env python3
"""Check that batsim --version is consistent with batsim git releases."""

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
    p = subprocess.run("{cmd} --version".format(cmd=batsim_command),
                       shell=True, check=True, stdout=subprocess.PIPE)

    return version_from_string(p.stdout.decode('utf-8'), pattern)


def batsim_git_version(
        git_command='git describe --match "v[0-9]*.[0-9]*.[0-9]*"',
        pattern='''(v\d+\.\d+\.\d+).*'''):
    """Retrieve the latest released Batsim version (in the git tags).

    This is done by calling git describe --match PATTERN,
    which should return either:
    - v1.2.0              if the current commit is a released version
    - v1.1.0-20-g0fcd0c9  if the current commit is after v1.1.0 in the tree
    - or fail if there is no released version before the current commit.
    """
    p = subprocess.run("{cmd}".format(cmd=git_command),
                       shell=True, stdout=subprocess.PIPE)

    if p.returncode != 0:
        raise Exception("Command '{cmd}' returned {ret}. Output:\n{o}".format(
            cmd=git_command, ret=p.returncode, o=p.stdout.decode('utf-8')))

    return version_from_string(p.stdout.decode('utf-8'), pattern)


def main():
    """Entry point."""
    bin_version = batsim_binary_version()
    git_version = batsim_git_version()

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
