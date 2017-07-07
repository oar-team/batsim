#!/usr/bin/env python3

"""Transforms a SWF to a Batsim workload with computation-only jobs."""

# Dependency : sortedcontainers
#    - installation: pip install sortedcontainers
# Everything else should be in the standard library
# Tested on cpython 3.4.3

from sortedcontainers import SortedSet
import argparse
import re
import json
import sys
import datetime

from swf import SwfField


def generate_workload(input_swf, output_json, computation_speed,
                      job_walltime_factor=2,
                      given_walltime_only=False,
                      job_grain=1,
                      platform_size=None,
                      indent=None,
                      translate_submit_times=False,
                      keep_only=False,
                      verbose=False,
                      quiet=False):
    """Generate a Batsim workload from a SWF trace."""
    element = '([-+]?\d+(?:\.\d+)?)'
    r = re.compile('\s*' + (element + '\s+') * 17 + element + '\s*')

    current_id = 0
    version = 0

    # Let a job be a tuple (job_id, nb_res, run_time, submit_time, profile,
    # walltime)

    jobs = []
    profiles = SortedSet()

    nb_jobs_discarded = 0
    minimum_observed_submit_time = float('inf')

    # Let's loop over the lines of the input file
    for line in input_swf:
        res = r.match(line)

        if res:
            job_id = (int(float(res.group(SwfField.JOB_ID.value))))
            nb_res = int(
                float(res.group(SwfField.ALLOCATED_PROCESSOR_COUNT.value)))
            run_time = float(res.group(SwfField.RUN_TIME.value))
            submit_time = max(0, float(res.group(SwfField.SUBMIT_TIME.value)))
            walltime = max(job_walltime_factor * run_time,
                           float(res.group(SwfField.REQUESTED_TIME.value)))

            if given_walltime_only:
                walltime = float(res.group(SwfField.REQUESTED_TIME.value))

            if nb_res > 0 and walltime > run_time and run_time > 0 and submit_time >= 0:
                profile = int(((run_time // job_grain) + 1) * job_grain)
                profiles.add(profile)

                job = (current_id, nb_res, run_time, submit_time, profile, walltime)
                current_id = current_id + 1
                minimum_observed_submit_time = min(minimum_observed_submit_time,
                                                   submit_time)
                jobs.append(job)
            else:
                nb_jobs_discarded += 1
                if verbose:
                    print('Job {} has been discarded'.format(job_id))

    # Export JSON
    # Let's generate a list of dictionaries for the jobs
    djobs = list()
    for (job_id, nb_res, run_time, submit_time, profile, walltime) in jobs:
        use_job = True
        if keep_only is not None:
            use_job = eval(keep_only)

        if use_job:
            djobs.append({'id': job_id,
                          'subtime': submit_time - minimum_observed_submit_time,
                          'walltime': walltime, 'res': nb_res,
                          'profile': str(profile)})

    # Let's generate a dict of dictionaries for the profiles
    dprofs = {}
    for profile in profiles:
        dprofs[str(profile)] = {'type': 'msg_par_hg',
                                'cpu': profile * computation_speed,
                                'com': 0.0}

    platform_size = max([nb_res for (job_id, nb_res, run_time, submit_time,
                                     profile, walltime) in jobs])
    if platform_size is not None:
        if platform_size < 1:
            print('Invalid input: platform size must be strictly positive')
            exit(1)
        platform_size = platform_size

    data = {
        'version': version,
        'command': ' '.join(sys.argv[:]),
        'date': datetime.datetime.now().isoformat(' '),
        'description': 'this workload had been automatically generated',
        'nb_res': platform_size,
        'jobs': djobs,
        'profiles': dprofs}

    try:
        outFile = open(output_json, 'w')

        json.dump(data, outFile, indent=indent, sort_keys=True)

        if not quiet:
            print('{} jobs and {} profiles had been created'.format(len(djobs),
                                                                    len(dprofs)))
            print('{} jobs have been discarded'.format(nb_jobs_discarded))
            if keep_only:
                print('{} jobs have been removed by keep_only'.format(
                    len(jobs) - len(djobs)))

    except IOError:
        print('Cannot write file', output_json)


def main():
    """
    Program entry point.

    Parses the input arguments then calls generate_flat_platform.
    """
    parser = argparse.ArgumentParser(
        description='Reads a SWF (Standard Workload Format) file and transform it into a JSON Batsim workload (with delay jobs)')
    parser.add_argument('input_swf', type=argparse.FileType('r'),
                        help='The input SWF file')
    parser.add_argument('output_json', type=str, help='The output JSON file')

    parser.add_argument('-cs', '--computation_speed',
                        type=float, required=True,
                        help='The computation speed of the machines, in number of floating-point operations per second. This parameter is used to convert logged seconds to a number of floating-point operations')

    parser.add_argument('-jwf', '--job_walltime_factor',
                        type=float, default=2,
                        help='Jobs walltimes are computed by the formula max(givenWalltime, jobWalltimeFactor*givenRuntime)')
    parser.add_argument('-gwo', '--given_walltime_only',
                        action="store_true",
                        help='If set, only the given walltime in the trace will be used')
    parser.add_argument('-jg', '--job_grain',
                        type=int, default=1,
                        help='Selects the level of detail we want for jobs. This parameter is used to group jobs that have close running time')
    parser.add_argument('-pf', '--platform_size', type=int, default=None,
                        help='If set, the number of machines to put in the output JSON files is set by this parameter instead of taking the maximum job size')
    parser.add_argument('-i', '--indent', type=int, default=None,
                        help='If set to a non-negative integer, then JSON array elements and object members will be pretty-printed with that indent level. An indent level of 0, or negative, will only insert newlines. The default value (None) selects the most compact representation.')
    parser.add_argument('-t', '--translate_submit_times',
                        action="store_true",
                        help="If set, the jobs' submit times will be translated towards 0")
    parser.add_argument('--keep_only',
                        type=str,
                        default=None,
                        help='If set, this parameter is evaluated to choose which jobs should be kept')

    group = parser.add_mutually_exclusive_group()
    group.add_argument("-v", "--verbose", action="store_true")
    group.add_argument("-q", "--quiet", action="store_true")

    args = parser.parse_args()

    generate_workload(input_swf=args.input_swf,
                      output_json=args.output_json,
                      computation_speed=args.computation_speed,
                      job_walltime_factor=args.job_walltime_factor,
                      given_walltime_only=args.given_walltime_only,
                      job_grain=args.job_grain,
                      platform_size=args.platform_size,
                      indent=args.indent,
                      translate_submit_times=args.translate_submit_times,
                      keep_only=args.keep_only,
                      verbose=args.verbose,
                      quiet=args.quiet)

if __name__ == "__main__":
    main()
