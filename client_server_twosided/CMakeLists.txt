cmake_minimum_required(VERSION 3.10)
project(client_server_twosided)

set(CMAKE_CXX_STANDARD 14)

add_definitions(-Wall -Wextra -Wpedantic -Wsign-conversion -Wunused)
add_definitions(-Wno-unused-function -Wno-deprecated-declarations)
add_definitions(-Wno-format -Wno-format-security)
add_definitions(-g -msse4.1)

# Libraries that are required:
set(LIBRARIES ${LIBRARIES} -pthread numa dl ibverbs ssl crypto erpc)


# Some paths:
# These two paths might need to be changed:
set(DPDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/dpdk_static)

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(TESTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)


# User options:
option(DEBUG "Enable debugging options" OFF) 
option(MEASURE_LATENCY "Measure latency" OFF)
option(MEASURE_THROUGHPUT "Server-side throughput measurement" OFF)
option(REAL_KV "Real KV-store at server for tests" OFF)
option(ENCRYPT "Enable encryption" ON)
set(TRANSPORT "dpdk" CACHE STRING "Datapath transport (infiniband/dpdk)")


if (DEBUG)
  message(STATUS "Debugging enabled. Compiling with address sanitizer")
  add_definitions(-fsanitize=address -Og)
  link_libraries(-fsanitize=address)
else()
  message(STATUS "Debugging disabled. Not compiling with address sanitizer")
endif()


if(MEASURE_LATENCY)
  add_definitions("-DMEASURE_LATENCY=true")
  message(STATUS "Latency measurement turned on")
else()
  add_definitions("-DMEASURE_LATENCY=false") 
  message(STATUS "Latency measurement turned off")
endif()

if(MEASURE_THROUGHPUT)
  add_definitions("-DMEASURE_THROUGHPUT=true")
  message(STATUS "Throughput measurement turned on")
else()
  add_definitions("-DMEASURE_THROUGHPUT=false")
  message(STATUS "Throughput measurement turned off")
endif()


if(REAL_KV)
  add_definitions("-DNO_KV_OVERHEAD=false")
  message(STATUS "Using std::map as a KV-store at the server")
else()
  add_definitions("-DNO_KV_OVERHEAD=true")
  message(STATUS "KV-store operations are constant")
endif()


if(ENCRYPT)
  message(STATUS "Encryption turned on")
  add_definitions("-DNO_ENCRYPTION=false")
else()
  message(STATUS "Warning: Encryption turned off")
  add_definitions("-DNO_ENCRYPTION=true")
endif()



if(TRANSPORT STREQUAL "infiniband")
  set(ERPC "${CMAKE_CURRENT_SOURCE_DIR}/eRPC4rdma")
  message(STATUS "Compiling with infiniband")
  add_definitions(-DERPC_INFINIBAND=true)

elseif(TRANSPORT STREQUAL "dpdk") 
  set(ERPC "${CMAKE_CURRENT_SOURCE_DIR}/eRPC4dpdk")
  message(STATUS "Compiling with DPDK")
  add_definitions(-DERPC_DPDK=true)

  set(DPDK_INCLUDE_DIR ${DPDK_PATH}/include/dpdk)
  message(STATUS "DPDK include directory = ${DPDK_INCLUDE_DIR}")
  include_directories(SYSTEM ${DPDK_INCLUDE_DIR})
  set(LIBRARIES 
	  ${LIBRARIES} -L${DPDK_PATH}/lib -Wl,--whole-archive dpdk -Wl,--no-whole-archive dl)

else()
    message(FATAL_ERROR "Invalid transport: ${TRANSPORT}")
endif()

message(STATUS "eRPC-path: ${ERPC}")


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
  ${SERVER_SOURCE})

target_compile_options(anchorserver
  PRIVATE -fPIC)

target_link_libraries(anchorserver 
  PRIVATE ${LIBRARIES})


add_library(anchorclient SHARED
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


add_executable(encryption_test
  ${TEST_UTILS}
  ${TESTS}/encryption_test.cpp)

target_link_libraries(encryption_test
  PRIVATE anchorserver)

