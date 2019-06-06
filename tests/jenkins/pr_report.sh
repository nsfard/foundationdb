#!/usr/bin/env bash

virtualenv -p python3.4 venv
source venv/bin/activate
git config --global user.name jenkins
git config --global user.email fdb-devs@snowflake.net
cd snowflake/jenkins
./build.sh sql_create_report
GIT_TREE=($(cd foundationdb && git rev-parse HEAD^{tree}))
cp -f fdb6-report.txt fdb6-report-${GIT_TREE}.txt