//
// Created by philip on 05.04.21.
//
#include "onesided_test_cli.h"
#include "onesided_client.h"


/* A possible implementation for a get function with the help of rdma_get
 * The array at value has to provide TEST_MAX_VALUE_SIZE bytes of space
 */
int get_function(const char *key, char *value) {
    struct local_key_info *info = get_key_info(key);
    if (!info)
        return -1;

    if (!rdma_get(info, (void *)value))
        return -1;
    else
        return 0;
}

/* A possible implementation for a put function with the help of rdma_put
 * The array at value has to have TEST_MAX_VALUE_SIZE bytes
 */
int put_function(const char *key, const char *value) {
    struct local_key_info *info = get_key_info(key);
    if (!info)
        return -1;

    return rdma_put(info, value);
}


int main(int argc, char **argv){
    if (parseargs(argc, argv))
        return -1;

    if (0 > rdma_client_connect(ip, port,
            key_do_not_use, sizeof(key_do_not_use), TEST_MAX_VALUE_SIZE))
        return -1;

    communicate(get_function, put_function);

    rdma_disconnect();
    return 0;
}
