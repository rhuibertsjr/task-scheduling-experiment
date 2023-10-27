APP=./bin/main

# Queue length
perf stat -e task:task_queue $APP &

# Throughput
perf record -e cycles -c 1 -a -g -- $APP &

# Context-switching
perf stat -e context-switches $APP &

wait
