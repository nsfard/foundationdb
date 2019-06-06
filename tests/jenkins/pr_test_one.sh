#!/usr/bin/env bash

# Designed to be run out of jenkins; PR_ITERATION is set to the index of this particular test

# Clean up the jenkins output; gets messy with too many iterations
set +x
exec 3>&1
exec 1> $WORKSPACE/setup_${PR_ITERATION}.log
exec 2>&1

virtualenv -p python3.4 venv
source venv/bin/activate
pip3 install docker-compose
docker-compose --version
git config --global user.name jenkins
git config --global user.email fdb-devs@snowflake.net

cd snowflake/jenkins
echo Iteration ${PR_ITERATION} building >&3
./build.sh configure download test sql sql_upload > $WORKSPACE/iteration_${PR_ITERATION}.log 2>&1
rc=$?
seed=$(find . -name traces.json -exec grep -m 1 CMakeSEED {} \; | awk '{print $2}'  | head -1 | tr -d '"}')
echo Iteration ${PR_ITERATION} completed with $rc - seed $seed >&3
mv $WORKSPACE/iteration_${PR_ITERATION}.log $WORKSPACE/iteration_${PR_ITERATION}_${seed}.log
find . -name traces.json -exec gzip -c {} > $WORKSPACE/traces_${PR_ITERATION}_${seed}.json.gz \;
