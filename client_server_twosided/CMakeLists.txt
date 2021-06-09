cmake_minimum_required(VERSION 3.13)
project(client_server_twosided)

set(CMAKE_CXX_STANDARD 14)

add_definitions(-Wall -Wextra -Wpedantic -Wsign-conversion -Wunused)
add_definitions(-Wno-unused-function -Wno-deprecated-declarations)
add_definitions(-msse4.1)

# Libraries that are required:
set(LIBRARIES ${LIBRARIES} -pthread numa dl ibverbs ssl crypto)


# Some paths:
# These two paths might need to be changed:
set(ERPC "${CMAKE_CURRENT_SOURCE_DIR}/eRPC4dpdk")
set(DPDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/dpdk_static)

message(STATUS "eRPC-path: ${ERPC}")

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(TESTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)


# User options:
set(DEBUG "OFF" CACHE STRING "Enable debugging options (ON/OFF)") 
set(MEASURE_LATENCY "ON" CACHE STRING "Measure latency (ON/OFF)")
set(ENCRYPT "ON" CACHE STRING "Enable encryption (ON/OFF)")
set(TRANSPORT "dpdk" CACHE STRING "Datapath transport (infiniband/dpdk)")


string(TOUPPER ${DEBUG} DEBUG)
if (${DEBUG} STREQUAL "ON")
  message(STATUS "Debugging enabled. Compiling with address sanitizer")
  add_definitions(-g -fsanitize=address)
  link_libraries(-fsanitize=address)
else()
  message(STATUS "Debugging disabled")
  add_definitions(-O2)
endif()


string(TOUPPER ${MEASURE_LATENCY} MEASURE_LATENCY)
if(${MEASURE_LATENCY} STREQUAL "ON")
  add_definitions("-DMEASURE_LATENCY=true")
  message(STATUS "Latency measurement turned on")
else()
  add_definitions("-DMEASURE_LATENCY=false") 
  message(STATUS "Latency measurement turned off")
endif()


string(TOUPPER ${ENCRYPT} ENCRYPT)
if(${ENCRYPT} STREQUAL "OFF")
  message(STATUS "Warning: Encryption turned off")
  add_definitions("-DNO_ENCRYPTION=true")
else()
  message(STATUS "Encryption turned on")
  add_definitions("-DNO_ENCRYPTION=false")
endif()



set(SOURCES)

if(TRANSPORT STREQUAL "infiniband")
  # Infiniband is nice and doesn't cause any problems
  message(STATUS "Compiling with infiniband")
  add_definitions(-DERPC_INFINIBAND=true)
  link_libraries(erpc)

elseif(TRANSPORT STREQUAL "dpdk") 
  message(STATUS "Compiling with DPDK")
  add_definitions(-DERPC_DPDK=true)


  # DPDK only works with an old version of eRPC
  # In this version, we have to compile eRPC along with our code to get
  # the best performance
  set(SOURCES ${SOURCES}
    ${ERPC}/src/nexus_impl/nexus.cc
    ${ERPC}/src/nexus_impl/nexus_bg_thread.cc
    ${ERPC}/src/nexus_impl/nexus_sm_thread.cc
    ${ERPC}/src/rpc_impl/rpc.cc
    ${ERPC}/src/rpc_impl/rpc_queues.cc
    ${ERPC}/src/rpc_impl/rpc_rfr.cc
    ${ERPC}/src/rpc_impl/rpc_cr.cc
    ${ERPC}/src/rpc_impl/rpc_kick.cc
    ${ERPC}/src/rpc_impl/rpc_req.cc
    ${ERPC}/src/rpc_impl/rpc_resp.cc
    ${ERPC}/src/rpc_impl/rpc_ev_loop.cc
    ${ERPC}/src/rpc_impl/rpc_fault_inject.cc
    ${ERPC}/src/rpc_impl/rpc_pkt_loss.cc
    ${ERPC}/src/rpc_impl/rpc_rx.cc
    ${ERPC}/src/rpc_impl/rpc_connect_handlers.cc
    ${ERPC}/src/rpc_impl/rpc_disconnect_handlers.cc
    ${ERPC}/src/rpc_impl/rpc_reset_handlers.cc
    ${ERPC}/src/rpc_impl/rpc_sm_api.cc
    ${ERPC}/src/rpc_impl/rpc_sm_helpers.cc
    ${ERPC}/src/transport_impl/transport.cc
    ${ERPC}/src/transport_impl/dpdk/dpdk_transport.cc
    ${ERPC}/src/transport_impl/dpdk/dpdk_transport_datapath.cc
    ${ERPC}/src/transport_impl/infiniband/ib_transport.cc
    ${ERPC}/src/transport_impl/infiniband/ib_transport_datapath.cc
    ${ERPC}/src/transport_impl/raw/raw_transport.cc
    ${ERPC}/src/transport_impl/raw/raw_transport_datapath.cc
    ${ERPC}/src/util/huge_alloc.cc
    ${ERPC}/src/util/externs.cc
    ${ERPC}/src/util/tls_registry.cc)


  find_package(Boost)
  if(Boost_FOUND)
    message(STATUS "Boost include directory = ${Boost_INCLUDE_DIRS}")
    include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
  else()
    message(FATAL_ERROR "Boost not found.")
  endif()
   
  set(DPDK_LIB ${DPDK_PATH}/lib/libdpdk.a)

  set(DPDK_INCLUDE_DIR ${DPDK_PATH}/include/dpdk)
  message(STATUS "DPDK include directory = ${DPDK_INCLUDE_DIR}")
  include_directories(SYSTEM ${DPDK_INCLUDE_DIR})
  set(LIBRARIES 
	  ${LIBRARIES} -L${DPDK_PATH}/lib -Wl,--whole-archive dpdk -Wl,--no-whole-archive dl)

else()
    message(FATAL_ERROR "Invalid transport: ${TRANSPORT}")
endif()


include_directories(SYSTEM ${ERPC}/src)
include_directories(${SRC} ${TESTS})

link_directories(${ERPC}/build)


set(SERVER_SOURCE
  ${SRC}/client_server_common.cpp
  ${SRC}/Server.cpp
  ${SRC}/Server.h
  ${SRC}/ServerThread.cpp
  ${SRC}/ServerThread.h)

set(CLIENT_SOURCE
  ${SRC}/client_server_common.cpp
  ${SRC}/Client.cpp
  ${SRC}/Client.h
  ${SRC}/PendingRequestQueue.cpp
  ${SRC}/PendingRequestQueue.h
  ${SRC}/sent_message_tag.cpp
  ${SRC}/sent_message_tag.h)

set(TEST_UTILS
  ${TESTS}/test_common.cpp
  ${TESTS}/test_common.h
  ${TESTS}/simple_unit_test.h)


# Finally build all tests and libraries:
add_library(anchorserver SHARED
  ${SOURCES}
  ${SERVER_SOURCE})

target_compile_options(anchorserver
  PRIVATE -fPIC)

target_link_libraries(anchorserver 
  PRIVATE ${LIBRARIES})


add_library(anchorclient SHARED
  ${SOURCES}
  ${CLIENT_SOURCE})

target_compile_options(anchorclient
  PRIVATE -fPIC)

target_link_libraries(anchorclient 
  PRIVATE ${LIBRARIES})


add_executable(server_test
  ${TEST_UTILS}
  ${TESTS}/server_test.cpp)

target_link_libraries(server_test
  PRIVATE anchorserver)


add_executable(client_test
  ${TEST_UTILS}
  ${TESTS}/client_test.cpp)

target_link_libraries(client_test
  PRIVATE anchorclient)


add_executable(server_perf_test
  ${TEST_UTILS}
  ${TESTS}/server_test_perf.cpp)

target_link_libraries(server_perf_test
  PRIVATE anchorserver pthread)


add_executable(client_perf_test
  ${TEST_UTILS}
  ${TESTS}/client_test_perf.cpp)

target_link_libraries(client_perf_test
    PRIVATE anchorclient pthread)

