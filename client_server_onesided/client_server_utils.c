#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>


#include "client_server_utils.h"


const char ERR_ENC[] = "Error: Something went wrong while encrypting";
const char ERR_DEC[] = "Error: Something went wrong while decrypting";

struct endpoint_data ep_data = { 0 };


/**
 * Initializes the rpma_peer structure that is needed by both, client and server
 * to communicate via RDMA
 *
 * @param ip Hostname to create context for
 * @param server Should be non-zero if called by server, 0 otherwise
 *
 * @return  0  on success
 *          -1 if some error occurs and the peer structure can't be initialized
 */
int initialize_peer(const char *ip, int server){
    /* ibv_ctx can be local because it's not needed after initialization */
    struct ibv_context *ibv_ctx;

    enum rpma_util_ibv_context_type type =
            server ? RPMA_UTIL_IBV_CONTEXT_LOCAL : RPMA_UTIL_IBV_CONTEXT_REMOTE;

    switch (rpma_utils_get_ibv_context(ip, type, &ibv_ctx)) {
        case 0: break;
        case RPMA_E_INVAL:
            PRINT_ERR("Invalid arguments: Cannot get IBV context");
            return -1;
        case RPMA_E_NOMEM:
            PRINT_ERR("Memory allocation failure");
            return -1;
        case RPMA_E_PROVIDER:
            PRINT_ERR("Network error");
            return -1;
        default:
            PRINT_ERR("Unknown error");
            return -1;
    }

    switch(rpma_peer_new(ibv_ctx, &(ep_data.peer))) {
        case 0: return 0;
        case RPMA_E_INVAL:
            PRINT_ERR("Could not create peer structure. Internal error");
            return -1;
        case RPMA_E_NOMEM:
            PRINT_ERR("Could not create peer structure. Memory allocation failure");
            return -1;
        case RPMA_E_PROVIDER:
            PRINT_ERR("Could not create peer structure. "
                            "Failed to create verbs protection domain");
            return -1;
        default:
            PRINT_ERR("Could not create peer structure. Unknown error");
            return -1;
    }
}


/**
 * Aligned malloc
 * Mostly taken from:
 * https://github.com/pmem/rpma/blob/master/examples/common/common-conn.c
 *
 * @param size Pointer to minimum size of the allocated memory; Updated with
 *          the actual size of the memory region
 * @return Pointer to allocated memory, NULL, if something went wrong
 */
void *malloc_aligned(size_t size){
    long pagesize = sysconf(_SC_PAGESIZE);
    if (pagesize < 0) {
        perror("sysconf");
        return NULL;
    }

    /* allocate a page size aligned local memory pool */
    void *mem;
    int ret = posix_memalign(&mem, (size_t)pagesize, size);
    if (ret) {
        (void) fprintf(stderr, "posix_memalign: %s\n", strerror(ret));
        return NULL;
    }

    return mem;
}


/**
 * Waits for the next event and checks, if it is an RPMA_CONN_ESTABLISHED event
 *
 * @param connection    Connection structure on which the event is obtained
 * @return              0  on success
 *                      -1 if an error occurs
 */
int establish_connection(struct rpma_conn *connection) {
    enum rpma_conn_event event;
    /* Wait for the "Connection established" event */
    if (rpma_conn_next_event(connection, &event) < 0){
        PRINT_ERR("Could not establish connection");
        return -1;
    }

    if (event != RPMA_CONN_ESTABLISHED){
        PRINT_ERR("Connection could not be established. Unexpected event: ");
        switch(event){
            case RPMA_CONN_CLOSED: PRINT_ERR("connection closed"); break;
            case RPMA_CONN_LOST: PRINT_ERR("connection lost"); break;
            case RPMA_CONN_UNDEFINED:
            default:
                PRINT_ERR("unknown connection event"); break;
        }
        return -1;
    }
    return 0;
}


/**
 * Ends a connection by waiting for the RPMA_CONN_CLOSED event and calling
 * rpma_conn_disconnect()
 * @param connection    connection to close
 * @param verbose       if not 0, error messages are printed to stderr
 * @return              0  if RPMA_CONN_CLOSED event was received
 *                      -1 if not
 */
int wait_for_disconnect_event() {
    enum rpma_conn_event event;
    /* Wait for the "Connection Closed" event */
    if (rpma_conn_next_event(ep_data.connection, &event) < 0) {
        PRINT_ERR("Error waiting for the next connection event");
        return -1;
    }

    if (event != RPMA_CONN_CLOSED) {
    PRINT_ERR("Connection could not be closed. Unexpected event: ");
        switch (event) {
            case RPMA_CONN_ESTABLISHED:
                PRINT_ERR("Connection established");
                break;
            case RPMA_CONN_LOST:
                PRINT_ERR("Connection lost");
                break;
            case RPMA_CONN_UNDEFINED:
            default:
                PRINT_ERR("Unknown connection event");
        }
        return -1;
    }

    return 0;
}


void free_endpoint_data() {
    if (ep_data.connection) {
        if (0 > rpma_conn_delete(&(ep_data.connection)))
            PRINT_ERR("Could not close connection");
        else
            ep_data.connection = NULL;
    }
    if (ep_data.endpoint) {
        if (0 > rpma_ep_shutdown(&(ep_data.endpoint)))
            PRINT_ERR("Could not shutdown endpoint");
        else
            ep_data.endpoint = NULL;
    }
    if (ep_data.memory_region) {
        if (0 > rpma_mr_dereg(&(ep_data.memory_region)))
            PRINT_ERR("Could not deregister memory region");
        else
            ep_data.memory_region = NULL;
    }
    if (ep_data.peer) {
        if (0 > rpma_peer_delete(&(ep_data.peer)))
            PRINT_ERR("Could not delete peer structure");
        else
            ep_data.peer = NULL;
    }
}

/*
int get_sequence_number(uint64_t *seq) {
    if (1 != RAND_bytes((unsigned char*) seq, sizeof(uint64_t))) {
        PRINT_ERR("Could not generate sequence number");
        return -1;
    }
    return 0;
}
 */


/**
 * Encrypts a value along with its context and writes the encrypted data to the
 * memory region pointed to by encrypted_entry
 * First the sequence number, then the value is encrypted
 * Afterwards, additional associated data is added in the following order:
 *  1. The key
 *  2. The address
 * @param encryption_key Key to use for encryption
 * @param info Filled-in local_key_info struct
 * @param new_value Value to encrypt
 * @param encrypted_entry Pointer to memory location where encrypted data
 *        should be stored.
 *        Must provide VALUE_ENTRY_SIZE(data->value_size) bytes of space
 * @return 0 on success, -1 on failure
 */
int encrypt_key_value_data(const unsigned char *encryption_key,
        const struct local_key_info *info,
        const void *new_value, unsigned char *encrypted_entry){

    if (!(info && encrypted_entry)){
                PRINT_ERR("Invalid parameters");
        return -1;
    }

    int ret = -1;
    int length;
    unsigned char *pos_encrypted_entry = encrypted_entry;

    /* Generate random IV at the beginning of the entry */
    if (1 != RAND_bytes(pos_encrypted_entry, IV_SIZE)) {
                PRINT_ERR("Could not generate IV");
        return -1;
    }

    EVP_CIPHER_CTX *aes_ctx = EVP_CIPHER_CTX_new();
    if (!aes_ctx){
                PRINT_ERR("Memory allocation failure");
        return -1;
    }

    /* Initialize Encryption; IV is at pos_encrypted_entry */
    if (1 != EVP_EncryptInit_ex(aes_ctx, EVP_aes_128_gcm(), NULL, encryption_key,
            pos_encrypted_entry)){
                PRINT_ERR("Failed to initialize encryption");
        ERR_print_errors_fp(stderr);
        goto end_encrypt;
    }

    /* Set IV length: */
    if (1 != EVP_CIPHER_CTX_ctrl(aes_ctx, EVP_CTRL_AEAD_SET_IVLEN, IV_SIZE, NULL)){
                PRINT_ERR("Failed to set IV length");
        ERR_print_errors_fp(stderr);
        goto end_encrypt;
    }
    pos_encrypted_entry += IV_SIZE;

    /* Add Key as additional authenticated data: */
    if (1 != EVP_EncryptUpdate(
            aes_ctx, NULL, &length, info->key, (int) info->key_size)) {
                PRINT_ERR("Could not add key as aad");
        ERR_print_errors_fp(stderr);
        goto end_encrypt;
    }

    /* Add Address as additional authenticated data: */
    if (1 != EVP_EncryptUpdate(
            aes_ctx, NULL, &length, (unsigned char*) &(info->value_offset),
            sizeof(info->value_offset))) {
                PRINT_ERR("Could not add address as aad");
        ERR_print_errors_fp(stderr);
        goto end_encrypt;
    }

    /* Encrypt sequence number: */
    if (1 != EVP_EncryptUpdate(
            aes_ctx, pos_encrypted_entry, &length,
            (unsigned char*)&(info->sequence_number),
            (int) sizeof(info->sequence_number))) {
                PRINT_ERR("Failed to encrypt sequence number");
        ERR_print_errors_fp(stderr);
        goto end_encrypt;
    }
    pos_encrypted_entry += (size_t)length;

    /* Encrypt value: */
    if (1 != EVP_EncryptUpdate(
            aes_ctx, pos_encrypted_entry, &length, new_value,
            ep_data.block_size - MIN_VALUE_SIZE)) {
                PRINT_ERR("Failed to encrypt value");
        ERR_print_errors_fp(stderr);
        goto end_encrypt;
    }
    pos_encrypted_entry += (size_t)length;

    /* Write final encrypted data: */
    if (1 != EVP_EncryptFinal_ex(aes_ctx, pos_encrypted_entry, &length)) {
                PRINT_ERR("Could not write final encrypted data");
        ERR_print_errors_fp(stderr);
        goto end_encrypt;
    }
    pos_encrypted_entry += (size_t)length;

    /* Write tag: */
    if (1 != EVP_CIPHER_CTX_ctrl(aes_ctx, EVP_CTRL_AEAD_GET_TAG, MAC_SIZE,
            pos_encrypted_entry)) {
                PRINT_ERR("Could not write tag");
        ERR_print_errors_fp(stderr);
        goto end_encrypt;
    }
    ret = 0;

end_encrypt:
    EVP_CIPHER_CTX_free(aes_ctx);

    return ret;
}


/**
 * Checks if the new sequence number is valid
 * @param info Must contain the expected sequence number
 * @param new_seq_number The new sequence number that was read
 * @return 0 if the sequence number is valid, -1 if not
 */
inline int check_seq_number(struct local_key_info* info, uint64_t new_seq_number) {
    return info->sequence_number == new_seq_number ? 0 : -1;
}


/**
 * Decrypts the value entry in the ciphertext using additionally stored
 * information in the structure local_key_info.
 * Checks the sequence number according to local_key_info
 * @param decryption_key The key to use for decryption
 * @param ciphertext The ciphertext with the key to decrypt
 * @param ciphertext_size The size of the ciphertext. Must be at least MIN_VALUE_SIZE
 *          Values with length 0 are supported
 * @param info Structure with information about address and key to use for
 *          additional authenticated data.
 *          Must contain the expected sequence number that is checked and updated
 * @param data struct to store decrypted data in
 * @return 0 on success, -1 on error
 */
int decrypt_key_value_data(const unsigned char *decryption_key,
           unsigned char *ciphertext, size_t ciphertext_size,
           struct local_key_info *info, struct key_value_data *data) {

    if (!(decryption_key && ciphertext && info && data && ciphertext_size >= MIN_VALUE_SIZE)) {
                return -1;
    }

    int ret = -1;
    const unsigned char *pos_ciphertext = ciphertext;
    size_t value_size = ciphertext_size - MIN_VALUE_SIZE;
    unsigned char *pos_value = NULL;
    int to_free = 0;

    EVP_CIPHER_CTX *aes_ctx = EVP_CIPHER_CTX_new();
    if (!aes_ctx){
                return -1;
    }

    /* Initialize decryption and set IV (located at the beginning of the ciphertext) */
    if (1 != EVP_DecryptInit_ex(aes_ctx, EVP_aes_128_gcm(), NULL, decryption_key,
                                pos_ciphertext)){
                ERR_print_errors_fp(stderr);
        goto end_decrypt;
    }

    /* Set the length of the IV: */
    if (1 != EVP_CIPHER_CTX_ctrl(aes_ctx, EVP_CTRL_AEAD_SET_IVLEN, IV_SIZE, NULL)){
                PRINT_ERR("Error while setting IV length");
        ERR_print_errors_fp(stderr);
        goto end_decrypt;
    }
    pos_ciphertext += IV_SIZE;

    /* Set the authentication tag (located at the end of the ciphertext): */
    if (1 != EVP_CIPHER_CTX_ctrl(aes_ctx, EVP_CTRL_AEAD_SET_TAG, MAC_SIZE,
                                 ciphertext + ciphertext_size - MAC_SIZE)) {
                PRINT_ERR("Error while setting authentication tag");
        ERR_print_errors_fp(stderr);
        goto end_decrypt;
    }

    /* Add Key as additional authenticated data: */
    int length;
    if (1 != EVP_DecryptUpdate(aes_ctx, NULL, &length, info->key, info->key_size)) {
                PRINT_ERR("Error while adding Key as aad");
        ERR_print_errors_fp(stderr);
        goto end_decrypt;
    }

    /* Add offset as additional authenticated data: */
    if (1 != EVP_DecryptUpdate(aes_ctx, NULL, &length,
            (unsigned char*) &(info->value_offset), sizeof(info->value_offset))) {
                PRINT_ERR("Error while adding Address as aad");
        ERR_print_errors_fp(stderr);
        goto end_decrypt;
    }

    /* Decrypt sequence number: */
    if (1 != EVP_DecryptUpdate(
            aes_ctx, (unsigned char*) &(data->sequence_number), &length,
            pos_ciphertext, sizeof(data->sequence_number))
            || length != sizeof(data->sequence_number)){
                PRINT_ERR("Error while decrypting sequence number");
        ERR_print_errors_fp(stderr);
        goto end_decrypt;
    }
    pos_ciphertext += (size_t)length;

    /* Decrypt value, if its size is at least one byte: */
    if (value_size == 0) {
        data->value = NULL;
        goto finish_decryption;
    }
    else {
        /* If data->value is NULL, we allocate memory for storing the value */
        if (!data->value) {
            data->value = malloc(value_size);
            if (!data->value) {
                PRINT_ERR("Memory allocation failure");
                goto end_decrypt;
            }
            to_free = 1;
        }
    }
    if (1 != EVP_DecryptUpdate(
            aes_ctx, data->value, &length, pos_ciphertext, value_size)
            || length < 0 || (size_t)length != value_size) {
                PRINT_ERR("Error while decrypting value");
        ERR_print_errors_fp(stderr);
        goto end_decrypt;
    }
    pos_value = (unsigned char *)data->value + (size_t)length;

finish_decryption:
    /* Finish decryption: */
    if (1 != EVP_DecryptFinal_ex(aes_ctx, pos_value, &length)){
                PRINT_ERR("Error: Could not verify authentication tag");
        ERR_print_errors_fp(stderr);
        goto end_decrypt;
    }
    if (info->sequence_number != data->sequence_number) {
        PRINT_ERR("Wrong sequence number");
        fprintf(stderr, "Expected: %zu, Found: %zu\n",
                info->sequence_number, data->sequence_number);
        goto end_decrypt;
    }
    ret = 0;  // TODO: check_seq_number(info, data->sequence_number);

end_decrypt:
    EVP_CIPHER_CTX_free(aes_ctx);
    if (ret && to_free)
        free(data->value);
    return ret;
}

/**
 * Gets a value from a key. A connection has to be initialized in advance
 * @param decryption_key Key to use for decryption
 * @param read_function Function that reads blocksize bytes at address from a KV-store,
 *          stores it to a buffer managed by the caller and returns a pointer
 *          to the buffer on success and NULL otherwise
 * @param info Pointer to structure with information that is needed to get the
 *          value from the server
 * @param value Pointer to a sufficiently big memory region at which the value
 *          should be stored. If value is NULL, memory for the value is allocated
 * @return Pointer to the location where the value is stored
 */
void *onesided_get(const unsigned char *decryption_key,
        unsigned char *(*read_function)(size_t),
        struct local_key_info *info, void *value) {

    if (!(decryption_key && info && ep_data.memory_region)) {
        PRINT_ERR("Invalid parameter!");
        return NULL;
    }

    struct key_value_data data;
    data.value = value;
    /*
     * The ciphertext is read into a buffer that is provided and managed
     * by the caller. This buffer should be returned by the read_function
     */
    unsigned char *ciphertext = read_function(info->value_offset);
    if (!ciphertext)
        return NULL;

    if (0 > decrypt_key_value_data(
            decryption_key, ciphertext, ep_data.block_size, info, &data))
        return NULL;

    return data.value;
}

/**
 * Performs a one-sided put-operation by encrypting the new value and inserting
 * it at the address specified in info
 * @param encryption_key Key to use for encryption
 * @param ciphertext_buf Buffer for ciphertext that is maintained by the caller
 * @param write_function Function that writes the contents in ciphertext_buf to the
 *          address specified in info
 * @param info Must contain current information about the address for the value
 *          and especially about the sequence number
 * @param new_value New value to encrypt and insert at the specified address
 * @return 0 on success, -1 otherwise
 */
int onesided_put(const unsigned char *encryption_key,
        unsigned char *ciphertext_buf,
        int (*write_function)(size_t),
        const struct local_key_info *info, const void *new_value) {

    if (!(encryption_key && ciphertext_buf && write_function && info && new_value)) {
        PRINT_ERR("onesided_put: Invalid parameter!");
    }

    /* Encrypt the message and store the data in the local memory region: */
    if (0 > encrypt_key_value_data(encryption_key, info, new_value, ciphertext_buf))
        return -1;

    /* Write the data with the write method that was passed: */
    return write_function(info->value_offset);
}
