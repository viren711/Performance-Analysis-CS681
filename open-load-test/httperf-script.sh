#!/bin/bash

SERVER="10.51.12.133"
PORT=80
URI="/test.php"
CONNS=5000

RATES=(200 300 400 500 600 700 800 900 1000 1100 1200)

echo "Starting httperf open-load experiments" > open-load-test.txt
echo "=====================================" >> open-load-test.txt

for RATE in "${RATES[@]}"
do
  echo "" >> open-load-test.txt
  echo "===== RATE: $RATE req/s =====" >> open-load-test.txt
  date >> open-load-test.txt

  httperf \
    --server $SERVER \
    --port $PORT \
    --uri $URI \
    --rate $RATE \
    --num-conns $CONNS >> open-load-test.txt

  sleep 20
done

echo "=====================================" >> open-load-test.txt
echo "All experiments completed" >> open-load-test.txt