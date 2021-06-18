#!/usr/bin/python3

import argparse
import os
import socket
import time


def run_single_test(key_size, value_size, max_key_size, threads,
                    puts, gets, deletes,
                    iterations, timeout,
                    csv_path):

    server.send("{},{},{}"
                .format(key_size, value_size, threads).encode())
    assert(server.recv(1024) == b"Server Running")

    time.sleep(2)

    command = "./client_perf_test"
    params = " {} {} -k {} -v {} -s {} -n {} -p {} -g {} -d {} -i {} -t {} -f {}"\
        .format(args.client_ip, args.server_ip,
                key_size, value_size, max_key_size, threads,
                puts, gets, deletes,
                iterations, timeout,
                csv_path)

    print("Executing " + command + params)
    os.system(command + params)
    server.send(b"Client terminated")
    assert(server.recv(1024) == b"Ready")


def different_threads_test(threads_list):
    for num_threads in threads_list:
        puts = args.p * 4 / num_threads
        gets = args.g * 4 / num_threads
        deletes = args.d * 4 / num_threads
        run_single_test(args.k, args.v, args.s, num_threads,
                        puts, gets, deletes,
                        args.i, args.t,
                        "threads_test.csv")
        num_threads *= 2


def network_performance_test(size_list):
    for size in size_list:
        run_single_test(size, size, args.s, args.n,
                        0, args.g, 0,
                        args.i, args.t,
                        "network_test.csv")


def different_sizes_test(size_list):
    for size in size_list:
        run_single_test(args.k, size, args.s, args.n,
                        args.p, args.g, args.d,
                        args.i, args.t,
                        "sizes_test.csv")


parser = argparse.ArgumentParser()
parser.add_argument("client_ip", type=str)
parser.add_argument("server_ip", type=str)
parser.add_argument("-k", type=int, help="key size")
parser.add_argument("-v", type=int, help="value size")
parser.add_argument("-s", type=int, help="maximum key size")
parser.add_argument("-n", type=int, help="number of threads/clients")
parser.add_argument("-t", type=int, help="client timeout on server overload in us")
parser.add_argument("-i", type=int, help="client event loop iterations")
parser.add_argument("-p", type=int, help="number of put operations per client")
parser.add_argument("-g", type=int, help="number of get operations per client")
parser.add_argument("-d", type=int, help="number of delete operations per client")

args = parser.parse_args()
print("Key size: {}, Value size: {}".format(args.k, args.v))
print("Client ip: {}. Server ip: {}".format(args.client_ip, args.server_ip))

server = socket.socket()
server.connect((args.server_ip, 31849))

network_performance_test([256, 512, 1024, 2048, 4096])

different_threads_test([1, 2, 4, 8])

different_sizes_test([256, 512, 1024, 2048, 4096])

server.send(b"END")

server.close()
exit(0)
