#!/usr/bin/env bash

set -e
set -x

virtualenv -p python3.4 venv
source venv/bin/activate
pip3 install docker-compose
docker-compose --version
git config --global user.name jenkins
git config --global user.email fdb-devs@snowflake.net
cd snowflake/jenkins
./build.sh check_uploaded package sql sql_upload upload