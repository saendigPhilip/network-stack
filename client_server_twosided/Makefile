#Notes:
#1. The make target depends on how eRPC was compiled:
#   * If DTRANSPORT=dpdk, use `make dpdk`
#   * If DTRANSPORT=raw, use `make raw`
#   * If DTRANSPORT=infiniband, use `make infiniband`
LIBS = -lerpc -lpthread -lnuma -ldl -lssl -lcrypto
FLAGS = -g -Wall -Wextra -Wpedantic 
OUT = build
SRC = src
TEST = tests
COMMON = $(SRC)/client_server_common.cpp
SERVER = $(SRC)/Server.cpp $(TEST)/server_test.cpp -o $(OUT)/server_test -I $(SRC)/
CLIENT = $(SRC)/Client.cpp $(TEST)/client_test.cpp -o $(OUT)/client_test -I $(SRC)/
ERPC = ..
INCLUDE_DPDK = -I /usr/usr/local/include/dpdk 
LINK_DPDK = -L /usr/usr/local/lib/ -ldpdk 

error:
	@echo "Please choose one of the following targets: infiniband, raw, dpdk, clean"
	@exit 2
infiniband:
	mkdir -p $(OUT)
	g++ $(FLAGS) -std=c++14 $(SERVER) $(COMMON) -isystem $(ERPC)/src -L $(ERPC)/build $(LIBS) -libverbs -DERPC_INFINIBAND=true #-fsanitize=address
	g++ $(FLAGS) -std=c++14 $(CLIENT) $(COMMON) -isystem $(ERPC)/src -L $(ERPC)/build $(LIBS) -libverbs -DERPC_INFINIBAND=true #-fsanitize=address
raw:                                                                          
	mkdir -p $(OUT)
	g++ $(FLAGS) -std=c++14 $(SERVER) $(COMMON) -isystem $(ERPC)/src -L $(ERPC)/build $(LIBS) -libverbs -DERPC_RAW=true
	g++ $(FLAGS) -std=c++14 $(CLIENT) $(COMMON) -isystem $(ERPC)/src -L $(ERPC)/build $(LIBS) -libverbs -DERPC_RAW=true
dpdk:                                                    
	mkdir -p $(OUT)
	g++ $(FLAGS) -std=c++14 $(SERVER) $(COMMON) -isystem $(ERPC)/src -L $(ERPC)/build $(LIBS) $(INCLUDE_DPDK) $(LINK_DPDK) -DERPC_DPDK=true -march=native
	g++ $(FLAGS) -std=c++14 $(CLIENT) $(COMMON) -isystem $(ERPC)/src -L $(ERPC)/build $(LIBS) $(INCLUDE_DPDK) $(LINK_DPDK) -DERPC_DPDK=true -march=native
clean:
	rm $(OUT)/server $(OUT)/client

