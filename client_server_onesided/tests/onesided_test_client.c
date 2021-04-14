//
// Created by philip on 14.04.21.
//
#include "onesided_client.h"
#include "onesided_test_all.h"
#include "simple_unit_test.h"


int main(int argc, char **argv) {
    if (0 > parseargs(argc, argv))
        return -1;

    if (0 > rdma_client_connect(ip, port, NULL, 0, TEST_KV_MAX_VAL_SIZE)) {
        return -1;
    }

    /* TODO: Implement further tests */

    rdma_disconnect();
    return 0;
}

