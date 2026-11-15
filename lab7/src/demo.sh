#!/bin/bash
# Демонстрация лабораторной №7: запускает TCP- и UDP-серверы, отправляет
# сообщения клиентами и показывает, что получили серверы. Запуск: ./demo.sh
make -s all || exit 1

./tcpserver --port 10050 --bufsize 100 > /tmp/lab7_tcp.log 2>&1 &
T=$!
./udpserver --port 20001 --bufsize 1024 > /tmp/lab7_udp.log 2>&1 &
U=$!
trap 'kill $T $U 2>/dev/null' EXIT
sleep 0.5

echo "== TCP: клиент отправляет строку, сервер печатает её у себя =="
echo "hello over TCP" | ./tcpclient --ip 127.0.0.1 --port 10050 --bufsize 100
sleep 0.3
echo "-- вывод tcpserver:"; cat /tmp/lab7_tcp.log

echo
echo "== UDP: клиент отправляет строку, сервер возвращает её обратно (эхо) =="
echo "hello over UDP" | ./udpclient --ip 127.0.0.1 --port 20001 --bufsize 1024
sleep 0.3
echo "-- вывод udpserver:"; cat /tmp/lab7_udp.log
