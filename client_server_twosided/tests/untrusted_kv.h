

const void *kv_get(const void *key, size_t key_size, size_t *data_len);

int kv_put(const void *key, size_t key_size, void *value, size_t value_size);

int kv_delete(const void *key, size_t key_size);

void initialize_kv_store();

void cleanup_kv_store();

void *mmap_helper(size_t size);

void munmap_helper(void *addr, size_t size);
