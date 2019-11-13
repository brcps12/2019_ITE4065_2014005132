#!/bin/bash

sysbench \
  --mysql-host=localhost \
  --mysql-port=12943 \
  --mysql-socket=./mariadb/server/inst/mysql.sock \
  --mysql-user=$USER \
  --mysql-db=sbtest \
  --report-interval=1 \
  --threads=10 \
  --table-size=10000 \
  --tables=1 \
  --time=60 \
  --db-driver=mysql \
  /usr/share/sysbench/oltp_read_write.lua \
  $1

