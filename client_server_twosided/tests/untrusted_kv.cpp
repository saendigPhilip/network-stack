#include <atomic>
#include <iostream>
#include <map>
#include <mutex>
#include <vector>
#include <unistd.h>
#include <sys/mman.h>

#include "client_server_common.h"
#include "generate_traces.h"
#include "test_common.h"
#include "untrusted_kv.h"

#define FLUSH_DELAY 220

#if SCONE
#define SYS_untrusted_mmap 1025

void *scone_kernel_mmap(void *addr, size_t length, int prot, int flags, int fd,
    off_t offset)
{
    return (void *)syscall(SYS_untrusted_mmap, addr, length, prot, flags,
        fd, offset);
    // printf("scone mmap syscall number : %d\n", SYS_untrusted_mmap);
}

void *mmap_helper(size_t size)
{
    return scone_kernel_mmap(nullptr, size, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON, -1, 0);
}

#else // SCONE
void *
mmap_helper(size_t size)
{
        return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON,
                    -1, 0);
}
#endif // SCONE

void munmap_helper(void *addr, size_t size) {
    munmap(addr, size);
}

std::vector<unsigned char *> allocated_areas;
unsigned char *current_pointer{nullptr};
size_t area_size{0};

std::map<std::vector<unsigned char>, unsigned char *> test_kv_store;
std::atomic_flag kv_flag{false};

thread_local unsigned char *val_buf{nullptr};


static unsigned char *untrusted_malloc(size_t size) {
    static std::mutex malloc_mutex;
    std::lock_guard<std::mutex> malloc_guard(malloc_mutex);
    if (!current_pointer ||
        static_cast<size_t>(
            (allocated_areas.back() + area_size -
             current_pointer)) < size) {

        auto *allocated =
            static_cast<unsigned char *>(mmap_helper(area_size));
        if (!allocated) {
            std::cerr << "mmap failed\n";
            return nullptr;
        }
        allocated_areas.emplace_back(allocated);
        current_pointer = allocated + size;
        return allocated;
    }
    else {
        unsigned char *ret = current_pointer;
        current_pointer = current_pointer + size;
        return ret;
    }
}

void lock_kv() {
    while (kv_flag.test_and_set())
        ;
}

void unlock_kv() {
    kv_flag.clear();
}


const void *kv_get(const void *key, size_t key_size, size_t *data_len) {
    void *ret = nullptr;
    auto *key_uc = static_cast<const unsigned char *>(key);
    auto vec = std::vector<unsigned char>();
    vec.assign(key_uc, key_uc + key_size);
    if (!val_buf)
        val_buf = static_cast<unsigned char *>(malloc(VAL_SIZE));

    lock_kv();
    auto iter = test_kv_store.find(vec);
    if (iter != test_kv_store.end()) {
        struct rdma_msg_header header;
        struct rdma_dec_payload payload = {nullptr, val_buf, 0ul};
        if (0 != decrypt_message(&header, &payload, iter->second,
            CIPHERTEXT_SIZE(VAL_SIZE))) {
            std::cerr << "Decrypting failed\n";
        }
        *data_len = VAL_SIZE;
        ret = val_buf;
    }
    else
        *data_len = 0;
    unlock_kv();

    return ret;
}


int kv_put(const void *key, size_t key_size, void *value, size_t value_size) {
    int ret = -1;
    auto *key_uc = static_cast<const unsigned char *>(key);
    auto vec = std::vector<unsigned char>();
    vec.assign(key_uc, key_uc + key_size);
    auto iter = test_kv_store.find(vec);
    unsigned char *val;
    struct rdma_msg_header header{0ul, 0ul };
    struct rdma_enc_payload payload{
        nullptr, static_cast<unsigned char *>(value), value_size
    };

    lock_kv();
    if (iter == test_kv_store.end()) {
        val = untrusted_malloc(CIPHERTEXT_SIZE(VAL_SIZE));
        if (!val)
            goto end_kv_put;
        test_kv_store.insert({vec, val});
    }
    else {
        val = iter->second;
    }
    if (0 != encrypt_message(&header, &payload, &val)) {
        std::cerr << "Encrypting message for untrusted memory failed\n";
        goto end_kv_put;
    }
    // Simulate flush delay:
    volatile int i;
    for (i = 0; i < FLUSH_DELAY; i++);

    ret = 0;
end_kv_put:
    unlock_kv();
    return ret;
}

int kv_delete(const void *key, size_t key_size) {
    int ret = -1;
    auto *key_uc = static_cast<const unsigned char *>(key);
    auto vec = std::vector<unsigned char>();
    vec.assign(key_uc, key_uc + key_size);
    lock_kv();
    auto iter = test_kv_store.find(vec);
    if (iter != test_kv_store.end()) {
        test_kv_store.erase(iter);
        free(iter->second);
        ret = 0;
    }
    unlock_kv();
    return ret;
}

void initialize_kv_store() {
    unlock_kv();
    area_size = CIPHERTEXT_SIZE(VAL_SIZE) * (1 << 20);
}

void cleanup_kv_store() {
    for (void *ptr : allocated_areas) {
        munmap(ptr, area_size);
    }
}

