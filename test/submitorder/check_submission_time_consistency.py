#!/usr/bin/env python3
import json
import sys
import pandas as pd

# Let's get when jobs have been released
jobs = pd.read_csv('{batprefix}_jobs.csv'.format(batprefix=sys.argv[1]))
# Sorted by (submit_time, job_id)
jobs.sort_values(by=['submission_time', 'job_id'], inplace=True)
theoretical_order = [int(x) for x in jobs['job_id']]

# Let's now read in which order the jobs have been put into the socket
msg_file = open('{output_directory}/messages.txt'.format(
    output_directory=sys.argv[1].replace('/out', '')))
msg_str = msg_file.read()

msgs = msg_str.strip().split('\n')

socket_order = []
for msg in msgs:
    json_msg = json.loads(msg)
    for event in json_msg['events']:
        if event['type'] == 'JOB_SUBMITTED':
            job_str = event['data']['job_id']
            socket_order.append(int(job_str.split('!')[1]))

print('Jobs orders:')
print('  Theoretical:', theoretical_order)
print('  Socket:', socket_order)

if socket_order != theoretical_order:
    print('Mismatch...')
    sys.exit(1)
else:
    sys.exit(0)
