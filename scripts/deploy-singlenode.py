#!python3

import os
import argparse
import subprocess
import json

#
# Command line options
#
parser = argparse.ArgumentParser(
    description='Deploy TCPSPSuite on a slurm managed cluster.')
parser.add_argument('--work-dir', metavar='work_dir', type=str, required=True,
                    help='The directory to output data into. This directory should be empty.')
parser.add_argument('--tcpsp-bin', metavar='tcpsp_bin', type=str, required=True,
                    help='The path to the tcpspsuite binary')
parser.add_argument('--cpus-per-node',
                    metavar='cpus_per_node', type=int, required=False, default=16)
parser.add_argument('--mb-per-cpu', metavar='mb_per_cpu',
                    type=int, required=False, default=4000)
parser.add_argument('--time-per-step',
                    metavar='time_per_step', type=int, required=True)
parser.add_argument('--hchunks', metavar='hchunks', type=int, required=True,
                    help="How many horizontal chunks / partitions to use. For singlenode deployments, this translates into nodes to be used.")
parser.add_argument('--vchunks', metavar='vchunks', type=int, required=True,
                    help="How many vertical chunks / steps to use. This translates into a job chain.")
parser.add_argument('--job-id', metavar='job_id', type=str, default='')
parser.add_argument('args', nargs=argparse.REMAINDER)


args = parser.parse_args()


# Make output directory
if not os.path.isdir(args.work_dir):
    os.makedirs(args.work_dir)

if len(args.args) > 1:
    BASE_CMD = [args.tcpsp_bin] + ['-u'] + args.args[1:]
else:
    BASE_CMD = [args.tcpsp_bin] + ['-u']

print("Base command:")


#
# === Sanity Checking =====
#
print("==================================")
print("===      Sanity Checking       ===")
print("==================================")


def get_parameter(param_flag):
    for i in range(0, len(args.args) - 1):
        if args.args[i] == param_flag:
            return args.args[i+1]
    return None


def sanity_error(msg):
    print("!!!!!!!!!! Sanity Check Error !!!!!!!!!!!!")
    print(msg)
    confirmation = None
    while confirmation not in set(('y', 'n')):
        confirmation = input("---> Continue anyway? (y/n)")
    if confirmation != 'y':
        exit(-1)


def validate_json(filename):
    with open(filename, 'r') as testfile:
        try:
            j = json.load(testfile)
        except json.JSONDecodeError:
            sanity_error("{} seems not to be valid JSON".format(filename))


# Check instances
instance_dir = get_parameter('-d')
instance_file = get_parameter('-f')
if (instance_dir is not None) and (instance_file is not None):
    sanity_error("Specified both -d and -f")
if instance_dir is not None:
    if not os.path.isdir(instance_dir):
        sanity_error("Instance directory seems not to be a directory")
elif instance_file is not None:
    if not os.path.isfile(instance_file):
        sanity_error("Instance file seems not to be a file.")
    validate_json(instance_file)
else:
    sanity_error("No input specified")

# Check config
config_file = get_parameter('-c')
if not os.path.isfile(config_file):
    sanity_error("No config file specified!")
validate_json(config_file)

parallelism_opt = get_parameter('-p')
if parallelism_opt is None:
    sanity_error("Looks like you have not specified -p")

unique_opt = get_parameter('-u')
if unique_opt is None:
    sanity_error("Looks like you have not specified -u")


#
# =========================
#

# Create run files for all hchunks
CHUNK_FILES = [[] for i in range(0, args.hchunks)]

for i in range(0, args.hchunks):
    for step in range(0, args.vchunks):
        CHUNK_RUN_FILE = os.path.join(
            args.work_dir, 'run_hchunk_{}_vchunk_{}.sh'.format(i, step))
        CHUNK_FILES[i].append(CHUNK_RUN_FILE)
        CHUNK_STORAGE = os.path.join(
            args.work_dir, 'storage_hchunk_{}.sqlite3'.format(i))
        if args.job_id != "":
            CHUNK_JOB_ID = "{}__hchunk_{}_vchunk_{}".format(
                args.job_id, i, step)
        else:
            CHUNK_JOB_ID = "hchunk_{}_vchunk_{}".format(i, step)

        LOG_FILE = os.path.join(
            args.work_dir, 'log__hchunk_{}_vchunk_{}.txt'.format(i, step))

        CHUNK_CMD = BASE_CMD + ['-s', CHUNK_STORAGE]
        CHUNK_CMD += ["--partition-count", str(args.hchunks)]
        CHUNK_CMD += ["--partition-number", str(i)]

        CHUNK_CMD += ["2>&1", "> {}".format(LOG_FILE)]

        if i == 0 and step == 0:
            print("First command looks like this:")
            print(" ".join(CHUNK_CMD))
            confirmation = None
            while confirmation not in set(('y', 'n')):
                confirmation = input("Does this look good? (y/n) ")
                print(confirmation)
            if confirmation != 'y':
                exit(-1)

        # Commands to dump the environment
        ENV_FILE = os.path.join(
            args.work_dir, 'environment_hchunk_{}_vchunk_{}.txt'.format(i, step))
        ENV_DUMP_COMMAND = "env > {} \n".format(ENV_FILE)
        CPU_FILE = os.path.join(
            args.work_dir, 'cpuinfo_hchunk_{}_vchunk_{}.txt'.format(i, step))
        CPU_DUMP_COMMAND = "cat /proc/cpuinfo > {} \n".format(CPU_FILE)
        MEM_FILE = os.path.join(
            args.work_dir, 'meminfo_hchunk_{}_vchunk_{}.txt'.format(i, step))
        MEM_DUMP_COMMAND = "cat /proc/meminfo > {} \n".format(MEM_FILE)
        DMESG_FILE = os.path.join(
            args.work_dir, 'dmesg_hchunk_{}_vchunk_{}.txt'.format(i, step))
        DMESG_DUMP_COMMAND = "dmesg > {}\n".format(DMESG_FILE)
        
        with open(CHUNK_RUN_FILE, 'w') as chunk_run_file:
            chunk_run_file.write("""#!/bin/bash
# MSUB -m bea
# MSUB -M slurm-notify@special.tinloaf.de

# 60 GB ulimit
ulimit -v 62914560

source /home/kit/iti/mt8793/.profile
module load compiler/gnu/7.1
module load devel/cmake/3.11.1

ulimit -a

""")
            chunk_run_file.write(ENV_DUMP_COMMAND)
            chunk_run_file.write(CPU_DUMP_COMMAND)
            chunk_run_file.write(MEM_DUMP_COMMAND)
            chunk_run_file.write(" ".join(CHUNK_CMD))

            # Output dmesg after the fact, to debug oom killings
            chunk_run_file.write(DMESG_DUMP_COMMAND)


# Submit all the hchunks
time_seconds = args.time_per_step % 60
time_minutes = int((args.time_per_step % (60*60)) / 60)
time_hours = int((args.time_per_step % (60*60*24)) / (60*60))
time_days = int((args.time_per_step / (60*60*24)))


for i in range(0, args.hchunks):

    # submit all the vchunks
    prev_job_id = None
    print("Submitting hchunk Nr. {} of {}".format(i, args.hchunks))

    for step in range(0, args.vchunks):
        chunk_file = CHUNK_FILES[i][step]

        SUBMIT_CMD = ["msub"]
        SUBMIT_CMD += ['-d',  args.work_dir]
        SUBMIT_CMD += ['-m', 'bea', '-M', 'slurm-notify@special.tinloaf.de']
        SUBMIT_CMD += ['-l', 'nodes=1:ppn={}'.format(args.cpus_per_node)]
        SUBMIT_CMD += ['-l', 'walltime={}:{:02d}:{:02d}:{:02d}'.format(
            time_days, time_hours, time_minutes, time_seconds)]
        SUBMIT_CMD += ['-l', 'pmem={}MB'.format(args.mb_per_cpu)]

        if args.job_id != "":
            JOB_ID = "{}__hchunk_{}_of_{}__vchunk_{}_of_{}".format(
                args.job_id, (i+1), args.hchunks, (step + 1), args.vchunks)
        else:
            JOB_ID = "hchunk_{}_of_{}__vchunk_{}_of_{}".format(
                (i+1), args.hchunks, (step + 1), args.vchunks)

        SUBMIT_CMD += ['-N', JOB_ID]

        if args.time_per_step >= 259200:
            SUBMIT_CMD += ['-q', 'verylong']
        else:
            SUBMIT_CMD += ['-q', 'singlenode']

        if prev_job_id is not None:
            SUBMIT_CMD += ['-l', 'depend=afterany:{}'.format(prev_job_id)]

        SUBMIT_CMD += [chunk_file]

        submission = subprocess.run(
            SUBMIT_CMD, check=True, universal_newlines=True, stdout=subprocess.PIPE)
        prev_job_id = submission.stdout.strip()
        print("      CMD: {}".format(" ".join(SUBMIT_CMD)))
        print("--- Submitted vchunk Nr. {} of {}. Job ID: {}".format(
            step, chunk_file, prev_job_id))
