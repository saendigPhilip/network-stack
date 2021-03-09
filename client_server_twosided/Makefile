#Notes:
#1. The make target depends on how eRPC was compiled:
#   * If DTRANSPORT=dpdk, use `make dpdk`
#   * If DTRANSPORT=raw, use `make raw`
#   * If DTRANSPORT=infiniband, use `make infiniband`
LIBS = -lerpc -lpthread -lnuma -ldl -lssl -lcrypto
FLAGS = -g -Wall -Wextra

error:
	@echo "Please choose one of the following targets: infiniband, raw, dpdk, clean"
	@exit 2
infiniband:
	g++ $(FLAGS) -std=c++11 -o server Server.cpp common.cpp common.h -isystem ../src -L ../build $(LIBS) -libverbs -DERPC_INFINIBAND=true
	g++ $(FLAGS) -std=c++11 -o client Client.cpp common.cpp common.h -isystem ../src -L ../build $(LIBS) -libverbs -DERPC_INFINIBAND=true
raw:                                                
	g++ $(FLAGS) -std=c++11 -o server Server.cpp common.cpp common.h -isystem ../src -L ../build $(LIBS) -libverbs -DERPC_RAW=true
	g++ $(FLAGS) -std=c++11 -o client Client.cpp common.cpp common.h -isystem ../src -L ../build $(LIBS) -libverbs -DERPC_RAW=true
dpdk:                                              
	g++ $(FLAGS) -std=c++11 -o server Server.cpp common.cpp common.h -isystem ../src -I /usr/local/include/dpdk -L ../build $(LIBS) -ldpdk -DERPC_DPDK=true -march=native
	g++ $(FLAGS) -std=c++11 -o client Client.cpp common.cpp common.h -isystem ../src -I /usr/local/include/dpdk -L ../build $(LIBS) -ldpdk -DERPC_DPDK=true -march=native
clean:
	rm server client

