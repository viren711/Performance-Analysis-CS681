#!/bin/bash

SERVER="10.51.29.70"
PORT=80
URI="/test.php"
CONNS=5000

RATES=(10 50 100 200 300 500 700 800 900 1000)

echo "Starting httperf open-load experiments" > output.txt
echo "=====================================" >> output.txt

for RATE in "${RATES[@]}"
do
  echo "" >> output.txt
  echo "===== RATE: $RATE req/s =====" >> output.txt
  date >> output.txt

  httperf \
    --server $SERVER \
    --port $PORT \
    --uri $URI \
    --rate $RATE \
    --num-conns $CONNS >> output.txt

  sleep 10
done

echo "=====================================" >> output.txt
echo "All experiments completed" >> output.txt
