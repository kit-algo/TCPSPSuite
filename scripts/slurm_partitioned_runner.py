#!python3

import os
import argparse
import subprocess

#
# Command line options
#
parser = argparse.ArgumentParser(
    description='Run TCPSPSuite on a slurm managed cluster.')
parser.add_argument('--output-dir', metavar='output_dir', type=str, required=True,
                    help='The directory to output data into. This directory should be empty.')
parser.add_argument('--tcpsp-bin', metavar='tcpsp_bin', type=str, required=True,
                    help='The path to the tcpspsuite binary')
parser.add_argument('--parallelism', metavar='parallelism', type=int, required=True,
                    nargs='?', default=False, help="Run <parallelism> many solvers in parallel.")
parser.add_argument('--run-id', metavar='run_id', type=str, required=True,
                    help='Set a unique run ID')
parser.add_argument('args', nargs=argparse.REMAINDER)


args = parser.parse_args()


NODE_LIST = os.environ['SLURM_NODELIST']
#NODE_LIST.sort()
OUR_NODE = os.environ['SLURMD_NODENAME']
OUR_INDEX = os.environ['SLURM_NODEID']
JOB_ID = os.environ['SLURM_JOBID']
RUN_ID = args.run_id
NODE_COUNT = os.environ['SLURM_JOB_NUM_NODES']

SLURM_NPROCS = os.environ['SLURM_NPROCS']
SLURM_NTASKS = os.environ['SLURM_NTASKS']
SLURM_MEM_PER_CPU = os.environ['SLURM_MEM_PER_CPU']
SLURM_NCPUS = os.environ['SLURM_CPUS_ON_NODE']

OUTPUT_DIR = args.output_dir
TCPSP_BIN = args.tcpsp_bin

our_prefix = '{}-{}__'.format(RUN_ID, OUR_INDEX)

OUTPUT_PREFIX = os.path.join(OUTPUT_DIR, our_prefix)

# Generate options for the binary
CMD_STORAGE = ['-s', OUTPUT_PREFIX + 'storage.sqlite3']
CMD_PARTITION_COUNT = ['--partition-count', NODE_COUNT]
CMD_PARTITION_NUMBER = ['--partition-number', str(OUR_INDEX)]
CMD_RUN_ID = ['-r', RUN_ID]
CMD_PARALLELISM = []
if args.parallelism:
    CMD_PARALLELISM = ['-p', str(args.parallelism)]


CMD = [TCPSP_BIN] + CMD_STORAGE + CMD_PARTITION_COUNT + \
    CMD_PARTITION_NUMBER + CMD_RUN_ID + CMD_PARALLELISM
if len(args.args) > 0:
    CMD += args.args[1:]

#
# Write information file
#
with open(OUTPUT_PREFIX + '{}-{}__info.txt'.format(OUR_NODE, JOB_ID), 'w') as infofile:
    infofile.write("Run ID: {}\n".format(RUN_ID))
    infofile.write("Job ID: {}\n".format(JOB_ID))
    infofile.write("Node ID: {} / Node Name: {}\n".format(OUR_INDEX, OUR_NODE))
    infofile.write("Node List: {}\n".format(', '.join(NODE_LIST)))
    infofile.write("Command: {}\n".format(' '.join(CMD)))
    infofile.write("Slurm NProcs: {} / Slurm NTasks: {} / Slurm NCPUs: {}\n".format(
        SLURM_NPROCS, SLURM_NCPUS, SLURM_NTASKS))
    infofile.write("Slurm Mem-per-CPU: {}\n".format(SLURM_MEM_PER_CPU))
    infofile.write("\n -------- Environment Dump ----------\n\n")
    for key, value in os.environ.items():
        infofile.write("{}: \t{}\n".format(key, value))


subprocess.run(CMD)
