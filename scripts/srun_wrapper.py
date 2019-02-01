#!python3

import os
import argparse
import subprocess

#
# Command line options
#
parser = argparse.ArgumentParser(
    description='Run TCPSPSuite via srun.')
parser.add_argument('--nodes', metavar='nodes', type=int, required=True,
                    help='The number of hosts / slurm tasks.')
parser.add_argument('args', nargs=argparse.REMAINDER)


args = parser.parse_args()

print("Running with {} tasks.".format(args.nodes))

JOB_ID = os.environ['SLURM_JOBID']
SUBMIT_DIR = os.environ['SLURM_SUBMIT_DIR']

CMD = ["srun",  \
       "--exclusive", \
       "--ntasks={}".format(args.nodes), \
       "--ntasks-per-node=1", \
       "--kill-on-bad-exit", \
       "--mem=0"] + args.args

#
# Write information file
#
with open(os.path.join(SUBMIT_DIR, 'srun_wrapper_{}__info.txt'.format(JOB_ID)), 'w') as infofile:
    infofile.write("Command: {}\n".format(' '.join(CMD)))
    infofile.write("\n -------- Environment Dump ----------\n\n")
    for key, value in os.environ.items():
        infofile.write("{}: \t{}\n".format(key, value))


subprocess.run(CMD)
