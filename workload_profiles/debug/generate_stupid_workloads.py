#!/usr/bin/env python3
import argparse
import json
import random

def generate_workload(nb_jobs, input_profiles, nb_res,
                      output_filename, indent):

    profile_json = json.load(open(input_profiles, 'r'))
    profile_names = [profile for profile in profile_json['profiles']]
    walltime = 4200

    content = { "nb_res": nb_res,
                "jobs": [ {"id":i, "subtime":i, "walltime":4200, "res":1,
                           "profile":random.choice(profile_names)}
                          for i in range(1, nb_jobs+1)
                ],
                "profiles":profile_json['profiles']
    }

    with open(output_filename, 'w') as f:
        json.dump(content, f, indent=indent, sort_keys=True)

def main():
    script_description = "Generates a lot of delay profiles into a file."

    p = argparse.ArgumentParser(description=script_description)

    p.add_argument('-n', '--nb-jobs',
                   type=int,
                   default=100,
                   help='The number of different delay profiles to generate')

    p.add_argument('-p', '--input-profiles',
                   type=str,
                   required=True,
                   help='The input json file containing profiles')

    p.add_argument('--nb-res',
                   type=int,
                   default=32,
                   help='The number of resources')

    p.add_argument('-o', '--output',
                   type=str,
                   required=True,
                   help='The output file name')

    p.add_argument('-i', '--indent',
                   type=int,
                   default=None,
                   help='The output json indent')

    args = p.parse_args()
    generate_workload(args.nb_jobs, args.input_profiles, args.nb_res,
                      args.output, args.indent)

if __name__ == '__main__':
    main()

