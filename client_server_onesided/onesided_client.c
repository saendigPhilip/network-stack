#include <stdio.h>
#include <stdlib.h>

#include "client_server_utils.h"
#include "onesided_client.h"


unsigned char *local_buf = NULL;
size_t local_buf_size;

struct rpma_mr_remote *shared_remote = NULL;
size_t shared_remote_size;



int setup_local_buffer(size_t max_value_size) {
    ep_data.block_size =
            max_value_size ? VALUE_ENTRY_SIZE(max_value_size) : DEFAULT_LOCAL_SIZE;

    ep_data.memory_region_size = ep_data.block_size;

    /* Provide a local buffer for RDMA */
    local_buf = (unsigned char*) malloc_aligned(ep_data.memory_region_size);
    if (!local_buf) {
        PRINT_ERR("Memory allocation failure");
        return -1;
    }

    /* Register the previously allocated buffer: */
    int local_buf_usage = RPMA_MR_USAGE_READ_DST | RPMA_MR_USAGE_WRITE_SRC |
            RPMA_MR_USAGE_FLUSH_TYPE_VISIBILITY;
    int ret = rpma_mr_reg(ep_data.peer, (void *)local_buf,
            ep_data.memory_region_size, local_buf_usage, &(ep_data.memory_region));
    if (0 > ret) {
        PRINT_ERR("Could not register local memory region");
        free(local_buf);
        local_buf = NULL;
        switch (ret) {
            case RPMA_E_INVAL: PRINT_ERR("Invalid parameter"); break;
            case RPMA_E_NOMEM: PRINT_ERR("Memory allocation failure"); break;
            case RPMA_E_PROVIDER: PRINT_ERR("Memory registration failure"); break;
            default: PRINT_ERR("Unknown error");
        }
        return -1;
    }
    else
        return 0;
}


/**
 * Connects to an rpma-Server at ip:port
 * @param ip Hostname of the Server
 * @param port Port for communication
 * @param enc_key Key to use for encryption/decryption with AES_128_GCM.
 *          Must not be freed as long as encryption/decryption operations
 *          (with get/put) are performed
 * @param enc_key_length Length of Key for AES_128_GCM
 * @param max_val_size The maximum size of a value in the KV-Store
 *          If it is 0, DEFAULT_LOCAL_SIZE bytes are allocated.
 * @return 0 if a connection could be established, -1 otherwise
 */
int rdma_client_connect(const char *ip, const char *port,
        const unsigned char *enc_key, size_t enc_key_length, size_t max_val_size) {
    int ret = -1;
    struct rpma_conn_req *request = NULL;
    struct rpma_conn_private_data remote_private_data;
    struct common_data *data;

    if (initialize_peer(ip, 0))
        return -1;

    if (0 > setup_local_buffer(max_val_size)) {
        return -1;
    }
    ep_data.enc_key = enc_key;
    ep_data.enc_key_length = enc_key_length;

    /* Create a new connection request and connect: */
    if (0 > rpma_conn_req_new(ep_data.peer,
            ip, port, ep_data.config, &request)){
        PRINT_ERR("Error creating connection request");
        goto end_rdma_client_connect;
    }

    printf("Connecting to server at %s, port %s...\n", ip, port);
    ret = rpma_conn_req_connect(&request, NULL, &(ep_data.connection));
    if (0 > ret) {
        PRINT_ERR("Error while issuing connection request");
        switch (ret) {
            case RPMA_E_INVAL:
                fprintf(stderr, "Invalid parameter; request: %p, NULL, connection: %p\n",
                        (void *)request, (void *)ep_data.connection);
                break;
            case RPMA_E_NOMEM: PRINT_ERR("Memory allocation failure"); break;
            case RPMA_E_PROVIDER: PRINT_ERR("Initiating connection request failed"); break;
            default: fprintf(stderr, "Unknown error: %d", ret);
        }
        goto end_rdma_client_connect;
    }
    ret = -1;

    if (0 > establish_connection(ep_data.connection))
        goto end_rdma_client_connect;
    else {
        PRINT_INFO("Connected to server!");
    }

    /* As in the librpma examples, the address is transferred via private data */
    /* TODO: It's better to do this in the initial handshake */
    if (0 > rpma_conn_get_private_data(ep_data.connection, &remote_private_data)) {
        PRINT_ERR("Server didn't specify remote memory location");
        goto end_rdma_client_connect;
    }
    data = remote_private_data.ptr;

    if (0 > rpma_mr_remote_from_descriptor(
            data->descriptors, data->mr_desc_size, &shared_remote)) {
        PRINT_ERR("Could not get remote shared memory");
        goto end_rdma_client_connect;
    }

    if (0 > rpma_mr_remote_get_size(shared_remote, &shared_remote_size))
        PRINT_ERR("Could not get size of remote shared memory");
    else
        ret = 0;

end_rdma_client_connect:
    if (ret)
        rdma_disconnect();

    rpma_conn_req_delete(&request);
    return ret;
}


unsigned char *rdma_read_from_server(size_t src_offset) {
    int ret = -1;
    struct rpma_completion completion;

    for (int i = 0; ret; i++) {
        if (i == 3) {
            PRINT_ERR("Failed reading from server!");
            return NULL;
        }
        ret = rpma_read(ep_data.connection,
                ep_data.memory_region, 0, shared_remote,
                src_offset, ep_data.block_size, RPMA_F_COMPLETION_ALWAYS, NULL);

        /*
         * TODO: rpma_read is not blocking.
         *  So we could let the user decide, when he wants to get the read results
         */
        if (0 > rpma_conn_completion_wait(ep_data.connection)) {
            PRINT_ERR("Error while waiting for read completion");
            return NULL;
        }
        if (0 > rpma_conn_completion_get(ep_data.connection, &completion)) {
            PRINT_ERR("Could not get completion for read operation");
            return NULL;
        }
        if (completion.op != RPMA_OP_READ) {
            PRINT_ERR("Unexpected operation");
            return NULL;
        }
        if (completion.op_status != IBV_WC_SUCCESS) {
            PRINT_ERR("Read operation was not successful. Retrying");
            ret = -1;
        }
    }
    return local_buf;
}


/**
 * Gets a value from a key. A connection has to be initialized in advance
 * @param info Pointer to structure with information that is needed to get the
 *          value from the server
 * @param value Pointer to a sufficiently big memory region at which the value
 *          should be stored. If value is NULL, memory for the value is allocated
 * @return Pointer to the location where the value is stored
 */
void *rdma_get(struct local_key_info *info, void *value) {
    return onesided_get(rdma_read_from_server, info, value);
}

/* Directly writes a message from the local message buffer to the server */
int rdma_write_to_server(size_t offset) {
    struct rpma_completion completion;
    /* TODO: This can be done asynchronously, but then without guarantees */
    if (0 > rpma_write(ep_data.connection, shared_remote, offset,
            ep_data.memory_region, 0, ep_data.block_size,
            RPMA_F_COMPLETION_ALWAYS, NULL)) {
        return -1;
    }

    if (0 > rpma_conn_completion_wait(ep_data.connection)) {
        PRINT_ERR("Waiting for write completion failed");
        return -1;
    }
    if (0 > rpma_conn_completion_get(ep_data.connection, &completion)) {
        PRINT_ERR("Could not get information about completion");
        return -1;
    }
    if (completion.op != RPMA_OP_WRITE) {
        PRINT_ERR("Unexpected operation while writing");
        return -1;
    }
    if (completion.op_status != IBV_WC_SUCCESS) {
        PRINT_ERR("Could not successfully write data to server");
        return -1;
    }
    return 0;
}

/**
 * Puts a new value for a certain key to the remote KV-store
 * @param info Structure containing information about the Key
 *          Sequence number gets updated
 * @param new_value New value to be inserted for the key from info
 * @return 0 on success, -1 otherwise
 */
int rdma_put(struct local_key_info *info, const void *new_value) {
    info->sequence_number++;
    if (0 > onesided_put(local_buf, rdma_write_to_server, info, new_value)) {
        info->sequence_number--;
        return -1;
    }
    return 0;
}


void free_client() {
    free_endpoint_data();
    free(local_buf);
}


void rdma_disconnect() {
    if (rpma_mr_remote_delete(&shared_remote))
        PRINT_ERR("Could not free shared memory region struct");

    PRINT_INFO("Disconnecting...");

    free_client();
}


