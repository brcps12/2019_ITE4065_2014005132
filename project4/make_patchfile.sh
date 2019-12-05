#!/bin/bash

BASE_COMMIT=8ed646
cd ./mariadb/server
git diff --no-prefix $BASE_COMMIT > ../../patchfile

