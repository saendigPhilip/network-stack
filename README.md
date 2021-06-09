# network-stack
Network stack for fast and secure networking with RDMA

## client_server_onesided
Networking with onesided RDMA with librpma 

### Building the onesided network stack
(Details will follow)

## client_server_twosided
Networking with twosided RDMA with eRPC and support for DPDK

### Building the twosided network stack
  * Dependencies: 
    * numa (libnuma-dev) 
    * ibverbs (libibverbs-dev)
    * openssl (libssl-dev)
    * eRPC (in submodule) 
    * dpdk (in submodule) 
    * libboost-dev (only needed for dpdk)
 * Clone the repo, go to client_server_twosided, get submodules (eRPC and dpdk) with 
   ```
   git submodule init
   git submodule update
   ```
 * For dpdk: compile dpdk. Go to dpdk, optionally remove kernel and app from GNUMakefile, then:
   ```
   make install T=x86_64-native-linuxapp-gcc DESTDIR=../dpdk_static
   ```
 * Build eRPC. Go to eRPC directory.
   * For dpdk this can be skipped. If something doesn't work, try:
     ```
     cmake . -DTRANSPORT=dpdk -DPERF=on
     ```
   * For infiniband:
     ```
     cmake . -DTRANSPORT=infiniband -DROCE=on -DPERF=off
     make -j $(nproc)
     ```
 * In the directory client_server_twosided, execute:
   ```
   mkdir build && cd build
   cmake .. -DTRANSPORT=<infiniband/dpdk>
   make -j $(nproc)
   ```
     
