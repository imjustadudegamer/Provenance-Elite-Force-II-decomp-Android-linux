#!/bin/sh
# The pool-sizing rule's offline table proof: includes the shipping
# q_jobs_workers.h and asserts the split over every core count a machine
# reports.  Never launches the game.
set -e

here=$( cd "$( dirname "$0" )" && pwd )

cd "$here"
gcc -std=gnu17 -O1 -Wall -o ./jobsizing_oracle jobsizing_oracle.c
./jobsizing_oracle
rc=$?
rm -f ./jobsizing_oracle
exit $rc
