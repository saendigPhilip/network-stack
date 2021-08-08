#!/usr/bin/python3

from client_test_utils import run_single_test, close_connection

ops = 10000000
time_ms = 80000
kThreads = 6

print("--------------------"
      "Testing throughput. "
      "Make sure, client and server were compiled with -DMEASURE_LATENCY=off -DMEASURE_THROUGHPUT=on"
      "--------------------\n")

print("--------------------"
      "Testing throughput with r/w ratio of 50/50 with multiple threads"
      "--------------------\n")

for thread in [1, 2, 4, 8]:
    run_single_test(key_size=512, value_size=512, max_key_size=0, threads=thread,
                    puts=0, gets=ops, deletes=0,
                    iterations=1,
                    csv_path="throughput_threads.csv", min_time=time_ms)

print("\n\n--------------------"
      "Finished Testing throughput with r/w ratio of 50/50 with multiple threads. "
      "Testing throughput with r/w ratio of 50/50"
      "--------------------\n")

for size in [64, 256, 1024, 2048, 4096]:
    run_single_test(key_size=size, value_size=size, max_key_size=0, threads=kThreads,
                    puts=0, gets=ops, deletes=0,
                    iterations=1,
                    csv_path="throughput_5050.csv", min_time=time_ms)

print("\n\n--------------------"
      "Finished testing throughput with r/w ratio of 50/50. "
      "Testing throughput for comparison with iperf"
      "--------------------\n")

for size in [64, 256, 1024, 2048, 4096]:
    run_single_test(key_size=0, value_size=size, max_key_size=0, threads=kThreads,
                    puts=ops, gets=0, deletes=0,
                    iterations=1,
                    csv_path="throughput_comp_iperf.csv", min_time=time_ms)

print("\n\n--------------------"
      "Finished testing throughput for comparison with iperf"
      "--------------------\n")

close_connection()
