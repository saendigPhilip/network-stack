#!/bin/bash

# for THREADS in 1 2 4 6
# do
# 	sleep 2
# 	sudo -E Hugepagesize=2097152 \
# 		LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu/:/usr/lib/gcc/x86_64-linux-gnu/7/ \
# 		SCONE_VERSION=1 SCONE_LOG=0 SCONE_NO_FS_SHIELD=1 SCONE_NO_MMAP_ACCESS=1 \
# 		SCONE_HEAP=3584M SCONE_LD_DEBUG=1 \
# 		/opt/scone/lib/ld-scone-x86_64.so.1 \
# 		../client_perf_test 10.243.29.181 10.243.29.182 \
# 		-k 2048 -v 2048 -s 0 -n $THREADS -p 0 -g 10000000 -d 0 -i 1 -t 80000 -f throughput_threads.csv
# done

for SIZE in 64 256 1024 2048 4096
do
	sleep 2
	sudo -E Hugepagesize=2097152 \
		LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu/:/usr/lib/gcc/x86_64-linux-gnu/7/ \
		SCONE_VERSION=1 SCONE_LOG=0 SCONE_NO_FS_SHIELD=1 SCONE_NO_MMAP_ACCESS=1 \
		SCONE_HEAP=3584M SCONE_LD_DEBUG=1 \
		/opt/scone/lib/ld-scone-x86_64.so.1 \
		../client_perf_test 10.243.29.181 10.243.29.182 \
		-k $SIZE -v $SIZE -s 0 -n 4 -p 0 -g 10000000 -d 0 -i 1 -t 80000 -f latency.csv
done
