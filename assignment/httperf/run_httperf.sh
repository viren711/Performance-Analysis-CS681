#!/bin/bash

SERVER="10.51.29.70"
PORT=80
URI="/test.php"
CONNS=5000

RATES=(800 900 1000)

echo "Starting httperf open-load experiments" > output2.txt
echo "=====================================" >> output2.txt

for RATE in "${RATES[@]}"
do
  echo "" >> output2.txt
  echo "===== RATE: $RATE req/s =====" >> output2.txt
  date >> output2.txt

  httperf \
    --server $SERVER \
    --port $PORT \
    --uri $URI \
    --rate $RATE \
    --num-conns $CONNS >> output2.txt

  sleep 10
done

echo "=====================================" >> output2.txt
echo "All experiments completed" >> output2.txt
