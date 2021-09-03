#!/usr/bin/python3

from client_test_utils import run_single_test, close_connection

# Minimum time for the tests to run (should be > 60s)
time_ms = 80000

# Number of operations after which the client checks if the Minimum time has passed
ops = 10000000

# Number of server/client threads
kThreads = 4

# Sizes for which the tests are performed
test_sizes = [64, 128, 256, 512, 1024, 2048, 4096]

# Name of the output file
csv_path = "latency.csv"

# print("--------------------"
#       "Testing latency. "
#       "Make sure, the client was compiled with -DMEASURE_LATENCY=on -DMEASURE_THROUGHPUT=off"
#       "--------------------\n")

print("\n\n--------------------"
      "Testing latency with {} threads"
      "--------------------\n".format(kThreads))

for size in test_sizes:
    run_single_test(key_size=0, value_size=size, max_key_size=0, threads=kThreads,
                    puts=ops, gets=0, deletes=0,
                    iterations=1,
                    csv_path=csv_path, min_time=time_ms)

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
