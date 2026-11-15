#!/bin/bash
# Демонстрация лабораторной №6: запускает 3 сервера, считает k! mod m клиентом,
# сверяет с python и останавливает серверы. Запуск: ./demo.sh [k] [mod]
K=${1:-100000}
MOD=${2:-1000000007}

make -s all || exit 1

./server --port 20001 --tnum 4 > /tmp/lab6_s1.log 2>&1 &
P1=$!
./server --port 20002 --tnum 2 > /tmp/lab6_s2.log 2>&1 &
P2=$!
./server --port 20003 --tnum 3 > /tmp/lab6_s3.log 2>&1 &
P3=$!
trap 'kill $P1 $P2 $P3 2>/dev/null' EXIT
sleep 0.5

echo "== client: k=$K mod=$MOD, серверы из servers.txt =="
./client --k "$K" --mod "$MOD" --servers servers.txt

echo "== сверка с python =="
python3 -c "import math; print('python: ', math.factorial($K) % $MOD)"

echo "== какие куски получили серверы =="
for i in 1 2 3; do echo "-- сервер $i:"; grep -E "Receive|Total" /tmp/lab6_s$i.log; done
