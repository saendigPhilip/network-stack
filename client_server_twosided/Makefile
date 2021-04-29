#Notes:
#1. The make target depends on how eRPC was compiled:
#   * If DTRANSPORT=dpdk, use `make dpdk`
#   * If DTRANSPORT=raw, use `make raw`
#   * If DTRANSPORT=infiniband, use `make infiniband`
LIBS = -lerpc -lpthread -lnuma -ldl -lssl -lcrypto -Wl,--whole-archive -ldpdk -Wl,--no-whole-archive
FLAGS = -g -Wall -Wextra -Wpedantic 
FLAGS_TEST = -O3 -Wall -Wextra -Wpedantic
OUT = build
SRC = src
TEST = tests
COMMON = $(SRC)/client_server_common.cpp
SERVER = $(SRC)/Server.cpp $(TEST)/server_test.cpp -o $(OUT)/server_test -I $(SRC)/
CLIENT = $(SRC)/Client.cpp $(TEST)/client_test.cpp -o $(OUT)/client_test -I $(SRC)/
SERVER_TEST = $(SRC)/Server.cpp $(TEST)/server_test_perf.cpp $(TEST)/test_common.cpp -o $(OUT)/server_test_perf -I $(SRC)/
CLIENT_TEST = $(SRC)/Client.cpp $(TEST)/client_test_perf.cpp $(TEST)/test_common.cpp -o $(OUT)/client_test_perf -I $(SRC)/
ERPC = ../../eRPC
INCLUDE_ERPC = -isystem $(ERPC)/src -L $(ERPC)/build
INCLUDE_DPDK = -I /usr/usr/local/include/dpdk -L /usr/usr/local/lib/ -ldpdk 

error:
	@echo "Please choose one of the following targets: infiniband, raw, dpdk, clean"
	@exit 2
infiniband:
	mkdir -p $(OUT)
	g++ $(FLAGS) -std=c++14 $(SERVER) $(COMMON) $(INCLUDE_ERPC) $(LIBS) -libverbs -DERPC_INFINIBAND=true #-fsanitize=address
	g++ $(FLAGS) -std=c++14 $(CLIENT) $(COMMON) $(INCLUDE_ERPC) $(LIBS) -libverbs -DERPC_INFINIBAND=true #-fsanitize=address
infiniband_test:
	g++ $(FLAGS_TEST) -std=c++14 $(SERVER_TEST) $(COMMON) $(INCLUDE_ERPC) $(LIBS) -libverbs -DERPC_INFINIBAND=true 
	g++ $(FLAGS_TEST) -std=c++14 $(CLIENT_TEST) $(COMMON) $(INCLUDE_ERPC) $(LIBS) -libverbs -DERPC_INFINIBAND=true
dpdk:                                                    
	mkdir -p $(OUT)
	g++ $(FLAGS) -std=c++14 $(SERVER) $(COMMON) $(INCLUDE_ERPC) $(INCLUDE_DPDK) $(LIBS) -DERPC_DPDK=true -march=native
	g++ $(FLAGS) -std=c++14 $(CLIENT) $(COMMON) $(INCLUDE_ERPC) $(INCLUDE_DPDK) $(LIBS) -DERPC_DPDK=true -march=native
dpdk_test:                                                    
	mkdir -p $(OUT)
	g++ $(FLAGS_TEST) -std=c++14 $(SERVER_TEST) $(COMMON) $(INCLUDE_ERPC) $(INCLUDE_DPDK) $(LIBS) -DERPC_DPDK=true -march=native
	g++ $(FLAGS_TEST) -std=c++14 $(CLIENT_TEST) $(COMMON) $(INCLUDE_ERPC) $(INCLUDE_DPDK) $(LIBS) -DERPC_DPDK=true -march=native
clean:
	rm $(OUT)/server_test $(OUT)/client_test $(OUT)/server_test_perf $(OUT)/client_test_perf

