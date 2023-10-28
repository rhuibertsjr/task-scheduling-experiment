APP=./bin/main

#perf record -e cycles -c 1 -a -g -- $APP &

# Context-switching
#perf stat -e cache-references, cache-misses $APP &
#perf stat -e context-switches $APP &

perf stat -e cache-references,cache-misses,context-switches $APP

wait
