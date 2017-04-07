#!/usr/bin/env python
import argparse
import os
import shutil
import sys

def clean_output_directory(dir):
    if os.path.isdir(dir):
        subdirs = [dir + '/' + x for x in ['instances', 'sweeper', 'results']]
        for subdir in subdirs:
            if os.path.isdir(subdir):
                shutil.rmtree(subdir)

def main():
    parser = argparse.ArgumentParser(description = 'Cleans a batsim execN '
                                     'output directory')

    parser.add_argument('dir',
                        type=str,
                        help='The directory to clean')

    args = parser.parse_args()
    clean_output_directory(args.dir)


if __name__ == "__main__":
    main()
