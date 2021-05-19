# CMakeLists.txt, mostly taken from eRPC: 
# https://github.com/erpc-io/eRPC/blob/35270933836410d6779f9f2d4e1d8aaa1291a5d8/CMakeLists.txt
cmake_minimum_required(VERSION 2.8)
project(twosided_network_stack)

include(CMakeDependentOption)

# Pick a compiler
#set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_COMPILER g++)

add_definitions(-std=c++11 -march=native)
add_definitions(-Wall -Wextra -pedantic -Wsign-conversion -Wold-style-cast)
add_definitions(-Wno-unused-function -Wno-nested-anon-types -Wno-keyword-macro -msse4.1)

set(LIBRARIES ${LIBRARIES} rt numa pthread gflags)

# Unit tests
enable_testing()
find_package(GTest REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/third_party)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

# liberpc will be compiled unless profile-guided optimization or LTO is enabled
set(COMPILE_ERPC_LIB ON)

# Options exposed to the user
set(TRANSPORT "dpdk" CACHE STRING "Datapath transport (infiniband/raw/dpdk)")
option(ROCE "Use RoCE if TRANSPORT is infiniband" OFF)
option(PERF "Compile for performance" ON)
set(PGO "none" CACHE STRING "Profile-guided optimization (generate/use/none)")
set(LOG_LEVEL "warn" CACHE STRING "Logging level (none/error/warn/info/reorder/trace/cc)") 
cmake_dependent_option(LTO "Use link time optimization" ON "PERF" OFF)

# Parse the user-exposed options
if(PERF)
  message(STATUS "Compilation optimized for performance.")
  set(DEBUG OFF)
  set(TESTING OFF)
else(PERF)
  message(STATUS "Compilation not optimized for performance.")
  set(DEBUG ON)
  set(TESTING ON)
endif(PERF)

# Profile-guided optimization
if(PGO STREQUAL "generate")
  message(STATUS "Profile-guided optimization (generate mode) is enabled. Performance will be low.")
  add_definitions(-fprofile-generate)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-generate")
  set(COMPILE_ERPC_LIB OFF)
elseif(PGO STREQUAL "use")
  message(STATUS "Profile-guided optimization (use mode) is enabled.")
  add_definitions(-fprofile-use -fprofile-correction)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-use -fprofile-correction")
elseif(PGO STREQUAL "none")
  message(STATUS "Profile-guided optimization is disabled.")
endif()

# Logging level
if(LOG_LEVEL STREQUAL "none")
  message(STATUS "Logging level = none.")
  add_definitions(-DERPC_LOG_LEVEL=0)
elseif(LOG_LEVEL STREQUAL "error")
  message(STATUS "Logging level = error.")
  add_definitions(-DERPC_LOG_LEVEL=1)
elseif(LOG_LEVEL STREQUAL "warn")
  message(STATUS "Logging level = warn.")
  add_definitions(-DERPC_LOG_LEVEL=2)
elseif(LOG_LEVEL STREQUAL "info")
  message(STATUS "Logging level = info.")
  add_definitions(-DERPC_LOG_LEVEL=3)
elseif(LOG_LEVEL STREQUAL "reorder")
  message(STATUS "Logging level = reorder. Warning: Performance will be low.")
  add_definitions(-DERPC_LOG_LEVEL=4)
elseif(LOG_LEVEL STREQUAL "trace")
  message(STATUS "Logging level = trace. Warning: Performance will be low.")
  add_definitions(-DERPC_LOG_LEVEL=5)
elseif(LOG_LEVEL STREQUAL "cc")
  message(STATUS "Logging level = cc. Warning: Performance will be low.")
  add_definitions(-DERPC_LOG_LEVEL=6)
else()
  message(STATUS "No logging level specified. Using warning level.")
  add_definitions(-DERPC_LOG_LEVEL=2)
endif()

# Debug mode
if(DEBUG)
  message(STATUS "Debugging is enabled. Perf will be low.")
else(DEBUG)
  message(STATUS "Debugging is disabled.")
  add_definitions(-DNDEBUG)
  add_definitions(-O2)
endif(DEBUG)

# Testing for packet loss, machine failure, etc
if(TESTING)
  message(STATUS "Testing is enabled. Performance will be low.")
  add_definitions(-DERPC_TESTING=true)
else(TESTING)
  message(STATUS "Testing is disabled, so tests may fail.")
  add_definitions(-DERPC_TESTING=false)
endif(TESTING)

# Link-time optimization
if(LTO)
  message(STATUS "LTO is enabled. eRPC library won't be compiled.")
  set(COMPILE_ERPC_LIB OFF)
  add_definitions(-flto)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
else(LTO)
  message(STATUS "LTO is disabled. Performance will be low.")
endif(LTO)

set(SRC "../../eRPC/src")

set(SOURCES
  ${SRC}/nexus_impl/nexus.cc
  ${SRC}/nexus_impl/nexus_bg_thread.cc
  ${SRC}/nexus_impl/nexus_sm_thread.cc
  ${SRC}/rpc_impl/rpc.cc
  ${SRC}/rpc_impl/rpc_queues.cc
  ${SRC}/rpc_impl/rpc_rfr.cc
  ${SRC}/rpc_impl/rpc_cr.cc
  ${SRC}/rpc_impl/rpc_kick.cc
  ${SRC}/rpc_impl/rpc_req.cc
  ${SRC}/rpc_impl/rpc_resp.cc
  ${SRC}/rpc_impl/rpc_ev_loop.cc
  ${SRC}/rpc_impl/rpc_fault_inject.cc
  ${SRC}/rpc_impl/rpc_pkt_loss.cc
  ${SRC}/rpc_impl/rpc_rx.cc
  ${SRC}/rpc_impl/rpc_connect_handlers.cc
  ${SRC}/rpc_impl/rpc_disconnect_handlers.cc
  ${SRC}/rpc_impl/rpc_reset_handlers.cc
  ${SRC}/rpc_impl/rpc_sm_api.cc
  ${SRC}/rpc_impl/rpc_sm_helpers.cc
  ${SRC}/transport_impl/transport.cc
  ${SRC}/transport_impl/dpdk/dpdk_transport.cc
  ${SRC}/transport_impl/dpdk/dpdk_transport_datapath.cc
  ${SRC}/transport_impl/infiniband/ib_transport.cc
  ${SRC}/transport_impl/infiniband/ib_transport_datapath.cc
  ${SRC}/transport_impl/raw/raw_transport.cc
  ${SRC}/transport_impl/raw/raw_transport_datapath.cc
  ${SRC}/util/huge_alloc.cc
  ${SRC}/util/externs.cc
  ${SRC}/util/tls_registry.cc
  src/client_server_common.cpp
  tests/test_common.cpp)

# Boost
find_package(Boost)
if(Boost_FOUND)
  message(STATUS "Boost include directory = ${Boost_INCLUDE_DIRS}")
  include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
else()
  message(FATAL_ERROR "Boost not found.")
endif()

# Transport-specific. Mellanox OFED drivers are the best choice for raw and
# infiniband, but they do not play well with DPDK. So we compile only one
# transport. Other transports are exluded using preprocessor macros.
string(TOUPPER ${TRANSPORT} DEFINE_TRANSPORT)
add_definitions(-DERPC_${DEFINE_TRANSPORT}=true)
message(STATUS "Selected transport = ${TRANSPORT}.")
set(CONFIG_IS_ROCE false)

if(TRANSPORT STREQUAL "dpdk")
  # find_library(DPDK_LIB dpdk)
  set(DPDK_LIB ../../eRPC/dpdk/x86_64-native-linuxapp-gcc/lib/libdpdk.a)
  if(NOT DPDK_LIB)
    message(FATAL_ERROR "DPDK library not found")
  endif()

  set(LIBRARIES ${LIBRARIES} -L../../eRPC/dpdk/x86_64-native-linuxapp-gcc/lib/ dpdk dl)
  set(CONFIG_TRANSPORT "DpdkTransport")
  set(CONFIG_HEADROOM 40)

  # DPDK include directory. Locating rte_config.h does not work on some systems.
  # Example: it may be kept in /usr/include/x86_64-linux-gnu/, and symlinked
  # from the real DPDK include directory (/usr/include/dpdk/).
  # find_path(DPDK_INCLUDE_DIR NAMES rte_ethdev.h PATH_SUFFIXES dpdk)
  set(DPDK_INCLUDE_DIR ../../eRPC/dpdk/x86_64-native-linuxapp-gcc/include/)
  if (DPDK_INCLUDE_DIR)
    message(STATUS "DPDK include directory = ${DPDK_INCLUDE_DIR}")
  else()
    message(FATAL_ERROR "DPDK include directory not found")
  endif()
  include_directories(SYSTEM ${DPDK_INCLUDE_DIR})
else()
  find_library(IBVERBS_LIB ibverbs)
  if(NOT IBVERBS_LIB)
    message(FATAL_ERROR "ibverbs library not found")
  endif()

  set(LIBRARIES ${LIBRARIES} ibverbs)
  if(TRANSPORT STREQUAL "raw")
    set(CONFIG_TRANSPORT "RawTransport")
    set(CONFIG_HEADROOM 40)
  elseif(TRANSPORT STREQUAL "infiniband")
    set(CONFIG_TRANSPORT "IBTransport")
    if(ROCE)
      set(CONFIG_HEADROOM 40)
      set(CONFIG_IS_ROCE true)
    else()
      set(CONFIG_HEADROOM 0)
      set(CONFIG_IS_ROCE false)
    endif()
  endif()
endif()


# Compile the library
if(COMPILE_ERPC_LIB)
  message(STATUS "Compiling eRPC as a library")
  add_library(erpc ${SOURCES})
else() # Compile our executable
  message(STATUS "Compiling network stack")

  include_directories(src/)

  add_executable(server_test
    ${SOURCES}
    src/Server.cpp
    src/ServerThread.cpp
    tests/server_test.cpp)

  add_executable(client_test
    ${SOURCES}
    src/Client.cpp
    tests/client_test.cpp)

  add_executable(server_test_perf
    ${SOURCES}
    src/Server.cpp
    src/ServerThread.cpp
    tests/server_test_perf.cpp)

  add_executable(client_test_perf
    ${SOURCES}
    src/Client.cpp
    tests/client_test_perf.cpp)
endif()

