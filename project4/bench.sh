#!/bin/bash

sysbench \
  --mysql-host=localhost \
  --mysql-port=12943 \
  --mysql-socket=./mariadb/server/inst/mysql.sock \
  --mysql-user=$USER \
  --mysql-db=sbtest \
  --report-interval=1 \
  --threads=8 \
  --table-size=1000000 \
  --tables=10 \
  --time=60 \
  --db-driver=mysql \
  /usr/share/sysbench/oltp_read_write.lua \
  $1

