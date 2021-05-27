cmake_minimum_required(VERSION 3.19)
project(client_server_twosided)

set(CMAKE_CXX_STANDARD 14)

add_definitions(-Wall -Wextra -Wpedantic -Wsign-conversion  -Wunused -DERPC_INFINIBAND=true)
set(LIBRARIES ${LIBRARIES} erpc pthread numa dl ibverbs ssl crypto)

set(eRPC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../eRPC")
message(STATUS "eRPC-path: ${eRPC_PATH}")

include_directories(SYSTEM ${eRPC_PATH}/src)
include_directories(src)
include_directories(tests)
link_directories(${eRPC_PATH}/build)
link_libraries(${LIBRARIES})

set(SERVER_SOURCE
        src/client_server_common.cpp
        src/Server.cpp
        src/Server.h
        src/ServerThread.cpp
        src/ServerThread.h)

set(CLIENT_SOURCE
        src/client_server_common.cpp
        src/Client.cpp
        src/Client.h)

set(TEST_UTILS
        tests/test_common.cpp
        tests/test_common.h
        tests/simple_unit_test.h)

add_executable(server_test
        ${SERVER_SOURCE}
        ${TEST_UTILS}
        tests/server_test.cpp)

add_executable(client_test
        ${CLIENT_SOURCE}
        ${TEST_UTILS}
        tests/client_test.cpp)

add_executable(server_perf_test
        ${SERVER_SOURCE}
        ${TEST_UTILS}
        tests/server_test_perf.cpp)

add_executable(client_perf_test
        ${CLIENT_SOURCE}
        ${TEST_UTILS}
        tests/client_test_perf.cpp)

add_library(anchorserver STATIC
        ${SERVER_SOURCE})

add_library(anchorclient STATIC
        ${CLIENT_SOURCE})

