import os
import shlex
import subprocess

TEST_INSTANCE_TIMEOUT = int(os.getenv('TEST_INSTANCE_TIMEOUT', '5'))
PLATFORM_DIR = os.environ['PLATFORM_DIR']
WORKLOAD_DIR = os.environ['WORKLOAD_DIR']
EDC_DIR = os.environ['EDC_LD_LIBRARY_PATH']

def prepare_instance(name: str, test_root_dir: str, platform: str, edc: str, workload: str=None, edc_init_content: str='', use_json: bool=False, batsim_extra_args: list[str]=None):
    output_dir = f'{test_root_dir}/{name}'
    os.makedirs(output_dir, exist_ok=True)

    edc_init_filename = f'{output_dir}/edc-init'
    with open(edc_init_filename, 'w') as f:
        f.write(edc_init_content)

    batsim_cmd = [
        'batsim',
        '--export', f'{output_dir}/batout/',
        '--platform', f'{PLATFORM_DIR}/{platform}.xml',
        '--edc-library-file', f'{EDC_DIR}/lib{edc}.so', str(int(use_json)), edc_init_filename
    ]

    if workload is not None:
        batsim_cmd.extend([
            '--workload', f'{WORKLOAD_DIR}/{workload}.json'
        ])

    if batsim_extra_args is not None:
        batsim_cmd += batsim_extra_args

    batsim_cmd_filename = f'{output_dir}/batsim.sh'
    descriptor = os.open(path=batsim_cmd_filename, flags=os.O_WRONLY|os.O_CREAT|os.O_TRUNC, mode=0o700)
    with open(descriptor, 'w') as f:
        f.write(shlex.join(batsim_cmd) + '\n')

    return batsim_cmd, output_dir

def run_batsim(batsim_cmd, output_dir, timeout=None):
    used_timeout = TEST_INSTANCE_TIMEOUT
    if timeout is not None:
        used_timeout = timeout

    with open(f'{output_dir}/batsim.stdout', 'w') as outfile:
        with open(f'{output_dir}/batsim.stderr', 'w') as errfile:
            p = subprocess.run(batsim_cmd, stdout=outfile, stderr=errfile, timeout=used_timeout)
    return p
