#!/bin/bash

for SIZE in 64 256 1024 2048 4096
do
	sudo -E Hugepagesize=2097152 \
		LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu/:/usr/lib/gcc/x86_64-linux-gnu/7/ \
		SCONE_VERSION=1 SCONE_LOG=0 SCONE_NO_FS_SHIELD=1 SCONE_NO_MMAP_ACCESS=1 \
		SCONE_HEAP=3584M SCONE_LD_DEBUG=1 \
		/opt/scone/lib/ld-scone-x86_64.so.1 \
		../server_perf_test 10.243.29.182 -k 0 -v $SIZE -n 1
	
done
