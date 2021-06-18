#!/usr/bin/python3

from sys import argv
import socket
import os
import signal
import time


def run_single_test(key_size, val_size, num_clients):
    command = "./server_perf_test"
    params = " {} -k {} -v {} -n {}"\
        .format(argv[1], key_size, val_size, num_clients)
    pid = os.fork()
    if pid == 0:
        print("Executing " + command + params)
        os.execvp(os.path.abspath(command), (command + params).split(" "))
    else:
        client.send(b"Server Running")
        print(client.recv(1024).decode())
        time.sleep(1)
        try:
            os.kill(pid, signal.SIGINT)
        except OSError:
            print("Server already terminated")
        except Exception as e:
            print(e)
            server_socket.close()
            exit(1)
        else:
            print("Sent SIGINT to server")
        client.send(b"Ready")


if len(argv) != 2 or argv[1] in ["-h", "-help", "--help"]:
    print("Expected server IP as argument")
    exit(1)

server_socket = socket.socket()
server_socket.bind((argv[1], 31849))
server_socket.listen()
(client, _) = server_socket.accept()


incoming = client.recv(1024)
while incoming != b"END":
    param_list = incoming.decode().split(",")
    run_single_test(int(param_list[0]), int(param_list[1]), int(param_list[2]))
    incoming = client.recv(1024)

client.close()
server_socket.close()
exit(0)
