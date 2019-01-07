#!/usr/bin/env python3
import json
import sys
import pandas as pd

msg_file = open('{output_directory}/messages.txt'.format(
    output_directory=sys.argv[1].replace('/out', '')))
msg_str = msg_file.read()

msgs = msg_str.strip().split('\n')

errors = []
for msg in msgs:
    json_msg = json.loads(msg)
    for event in json_msg['events']:
        if event['type'] == 'JOB_KILLED':
            job_data = event['data']
            if 'job_progress' not in job_data:
                errors.append(job_data + " do not contains job progress\n")
            #TODO test if this job is msg or delay of composed
            #and test the progress content type in regards

if errors:
    print(errors)
    sys.exit(1)
else:
    sys.exit(0)
