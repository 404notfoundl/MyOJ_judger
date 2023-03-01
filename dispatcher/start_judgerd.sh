#!/bin/bash
/root/projects/limiter/limiter
log_file=judger.log
pid_file=judger.pid
while true
do
pid="$(cat $pid_file)"
res="$(ps -e | grep -w $pid | grep -v "grep" | wc -l)"

if [ $res -eq 1 ]; then
# proc exist
echo "pid exist" > /dev/null # 占位用
else
# restart proc
./main > $log_file
fi
sleep 300
done