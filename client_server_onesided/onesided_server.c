#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "client_server_utils.h"
#include "onesided_server.h"


struct common_data data_region = { 0 };
struct rpma_conn_private_data private_data = { 0 };

void *key_store_local = NULL;

/** TODO: This method should be part of the interface
 * Registers a memory region for RDMA with the current peer
 * @param region Memory region to register
 * @param size Size of the memory region
 * @return 0 on success, negative value otherwise
 */
int register_shared_memory_region(unsigned char *shared, size_t shared_size) {
    int ret = -1;
    /* Clients read from (READ_SRC) and write to (WRITE_DST) the memory region: */
    const int shared_usage = RPMA_MR_USAGE_READ_SRC | RPMA_MR_USAGE_WRITE_DST |
            RPMA_MR_USAGE_FLUSH_TYPE_VISIBILITY;
    /* Register shared memory region */
    if (0 > rpma_mr_reg(ep_data.peer, (void*) shared,
            shared_size, shared_usage, &(ep_data.memory_region))){
        PRINT_ERR("Failed to register memory region");
        return -1;
    }

    size_t descriptor_size;
    if (0 > rpma_mr_get_descriptor_size(ep_data.memory_region, &descriptor_size)) {
        PRINT_ERR("Failed to get size of registered memory");
        goto end_register;
    }
    data_region.mr_desc_size = (uint8_t) descriptor_size;

    if (0 > rpma_mr_get_descriptor(ep_data.memory_region, data_region.descriptors)) {
        PRINT_ERR("Failed to get descriptor of registered memory");
        goto end_register;
    }

    private_data.ptr = &data_region;
    private_data.len = sizeof(struct common_data);
    ret = 0;

end_register:
    if (ret) {
        if (0 > rpma_mr_dereg(&(ep_data.memory_region)))
            PRINT_ERR("Error de-registering memory region");
    }
    return ret;
}

/*
 * TODO: So far, the server writes encrypted data directly to the KV store.
 *          If something goes wrong, the keystore is corrupt
 */
int server_write(size_t address) {
    (void)(address); // suppress unused warning
    return 0;
}


/**
 * Sets up an encrypted KV-store from an unencrypted one
 * @param keystore_plain Keystore data in plaintext
 * @param num_values Number of values in the keystore
 * @param value_size (Fixed) size of each of the values
 * @param kv_store_size If not NULL, the size of the KV-Store is stored there
 * @param infos Information about where to place the values.
 *      TODO: Find a solution for efficiently storing the infos
 * @return pointer to the encrypted keystore on success, NULL otherwise
 */
unsigned char *setup_kv_store(const unsigned char *enc_key, size_t enc_key_length,
        void* kv_store_plain, size_t num_values, size_t value_size,
        size_t* kv_store_size, struct local_key_info* infos) {

    ep_data.memory_region_size = num_values * VALUE_ENTRY_SIZE(value_size);
    unsigned char *kv_store_enc =
            (unsigned char*)malloc_aligned(ep_data.memory_region_size);
    if (!kv_store_enc) {
        puts("Memory allocation failure");
        return NULL;
    }
    ep_data.block_size = VALUE_ENTRY_SIZE(value_size);
    ep_data.enc_key = enc_key;
    ep_data.enc_key_length = enc_key_length;
    for (size_t i = 0; i < num_values; i++) {
        if (0 > onesided_put(
                ((uint8_t *)kv_store_enc) + i * VALUE_ENTRY_SIZE(value_size),
                server_write, infos + i,
                ((uint8_t *)kv_store_plain) + i * value_size))
            goto err_setup_kv_store;
    }
    if (kv_store_size)
        *kv_store_size = ep_data.memory_region_size;

    return kv_store_enc;

err_setup_kv_store:
    free(kv_store_enc);
    return NULL;
}


/**
 * Registers a persistent memory region for Remote Direct Memory Access
 * Recommended: Call setup_kv_store before
 * @param ip IP-Address to listen on
 * @param port Port to listen on
 * @param shared_region Persistent Memory region to share, must be aligned to pagesize
 * @param shared_region_size Size of the shared Persistent Memory region
 * @param enc_key Key that should be used for encryption and decryption of the
 *          shared memory region. Must not be freed while encryption and
 *          decryption operations are performed
 * @param enc_key_length Length of enc_key
 * @param max_val_size Maximum size a value can have
 * @return 0 on success, -1 otherwise
 */
int host_server(const char *ip, const char *port,
        void *shared_region, size_t shared_region_size, size_t max_val_size,
        const unsigned char *enc_key, size_t enc_key_length) {
    int ret = -1;

    if (initialize_peer(ip, 1)) {
        PRINT_ERR("Could not initialize peer struct");
        return ret;
    }

    if (register_shared_memory_region(shared_region, shared_region_size)) {
        PRINT_ERR("Could not register memory region");
        goto end_host_server;
    }
    ep_data.enc_key = enc_key;
    ep_data.enc_key_length = enc_key_length;

    if (rpma_ep_listen(ep_data.peer, ip, port, &(ep_data.endpoint))) {
        PRINT_ERR("Cannot listen on specified hostname/port");
    }
    else {
        key_store_local = shared_region;
        ep_data.block_size = VALUE_ENTRY_SIZE(max_val_size);
        ret = 0;
    }

end_host_server:
    if (ret)
        free_endpoint_data(&ep_data);
    return ret;
}

unsigned char *server_read(size_t offset) {
    return (unsigned char *)key_store_local + offset;
}

void *server_get(struct local_key_info *info, void *value) {
    return onesided_get(server_read, info, value);
}

int server_put(struct local_key_info *info, const void *new_value) {
    return onesided_put((unsigned char *)key_store_local + info->value_offset,
            server_write, info, new_value);
}

int accept_client() {
    struct rpma_conn_req *request;
    /* Wait for connection request */
    if (0 > rpma_ep_next_conn_req(ep_data.endpoint, ep_data.config, &request)) {
        PRINT_ERR("Something went wrong while accepting client request");
        return -1;
    }

    /* Accept client, send data */
    if (0 > rpma_conn_req_connect(&request, &private_data, &(ep_data.connection))) {
        PRINT_ERR("Could not connect to client");
        return -1;
    }

    if (0 > establish_connection(ep_data.connection)) {
        PRINT_ERR("Could not establish connection to client");
        return -1;
    }
    return 0;
}

void shutdown_rdma_server(void) {
    free_endpoint_data();
}



