#!/usr/bin/python3

from client_test_utils import run_single_test, close_connection

ops = 100000
time_ms = 30000

print("--------------------"
      "Testing latency. "
      "Make sure, the client was compiled with -DMEASURE_LATENCY=on -DMEASURE_THROUGHPUT=off"
      "--------------------\n")

print("\n\n--------------------"
      "Testing latency single-threaded"
      "--------------------\n")

for size in [64, 256, 1024, 2048, 4096]:
    run_single_test(size, size, max_key_size=0, threads=1,
                    puts=0, gets=ops, deletes=0,
                    iterations=1,
                    csv_path="latency_single.csv", min_time=time_ms)

# print("--------------------"
#       "Finished testing latency single-threaded. "
#       "Testing latency with different number of threads"
#       "--------------------")
# 
# 
# for thread in [1, 4, 7, 8]:
#     run_single_test(key_size=512, value_size=512, max_key_size=0, threads=thread,
#                     puts=0, gets=ops, deletes=0,
#                     iterations=1,
#                     csv_path="latency_multi.csv", min_time=time_ms)


print("--------------------"
      "Finished testing latency" #  multi-threaded"
      "--------------------")

close_connection()
