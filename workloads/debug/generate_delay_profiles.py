#!/usr/bin/env python3

"""Generate simple delay profiles."""

import argparse
import json


def generate_delay_profiles(nb_profiles, output_filename, indent):
    """Generate profiles into output_filename."""
    content = {"profiles":
               {"delay_{}".format(str(i).zfill(3)): {"type": "delay",
                                                     "delay": float(i)}
                for i in range(1, nb_profiles + 1)}}

    with open(output_filename, 'w') as f:
        json.dump(content, f, indent=indent, sort_keys=True)


def main():
    """
    Program entry point.

    Parses main parameters then calls generate_delay_profiles
    """
    script_description = "Generates a lot of delay profiles into a file."

    p = argparse.ArgumentParser(description=script_description)

    p.add_argument('-n', '--nb-profiles',
                   type=int,
                   default=100,
                   help='The number of different delay profiles to generate')

    p.add_argument('-o', '--output',
                   type=str,
                   required=True,
                   help='The output file name')

    p.add_argument('-i', '--indent',
                   type=int,
                   default=None,
                   help='The output json indent')

    args = p.parse_args()
    generate_delay_profiles(args.nb_profiles, args.output, args.indent)

if __name__ == '__main__':
    main()
