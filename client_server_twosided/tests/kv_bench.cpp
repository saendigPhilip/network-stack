#include <cassert>
#include <cstdint>

#include "untrusted_kv.h"
#include "kv_bench.h"


#define WORKLOAD_PATH_SIZE 1024
static const char *wl_dir_path = "../ycsb_traces";
static int zipf_exp = 99;
static int key_number = 100000;
constexpr int read_ratio{50};

thread_local unsigned char *value_buf;
thread_local unsigned char *key_buf;
struct workload_params wl_params = { 0 };

/*
 * keys_file_construct -- construct the file name
 */
static char *
wl_file_construct()
{
    char *ret = (char *)malloc(WORKLOAD_PATH_SIZE * sizeof(char));
    snprintf(ret, WORKLOAD_PATH_SIZE,
         "%s/simple_trace_w_%ld_k_%d_a_%.2f_r_%.2f.txt", wl_dir_path,
         wl_size, key_number, float(zipf_exp) / 100,
         float(read_ratio) / 100);
    return ret;
}

/*
 * map_custom_wl_trace_init -- custom workload init
 */
std::vector<anchor_tr_::Trace_cmd>
workload_trace_init(char *wl_file)
{
    std::string tr(wl_file);
    std::vector<anchor_tr_::Trace_cmd> trace = anchor_tr_::trace_init(tr);

    return trace;
}

void release_workload()
{
    munmap_helper(wl_params.keys, wl_size * sizeof(*(wl_params.keys)));
    munmap_helper(wl_params.op, wl_size * sizeof(*(wl_params.op)));
}

/*
 * data_structure_warmup -- initialize KV with keys of custom workload
 */



int workload_scan()
{
    int ret = 0;

    char *wl_file = wl_file_construct();
    std::vector<anchor_tr_::Trace_cmd> trace = workload_trace_init(wl_file);
    free(wl_file);

    assert(wl_size == trace.size());

    wl_params.keys =
        (uint64_t *)mmap_helper(wl_size * sizeof(*(wl_params.keys)));
    if (!wl_params.keys) {
        perror("malloc keys");
        goto err;
    }
    wl_params.op = (anchor_tr_::Trace_cmd::operation *)mmap_helper(
        wl_size * sizeof(*(wl_params.op)));
    if (!wl_params.op) {
        perror("malloc op");
        goto err_free_ops;
    }

    for (uint64_t i = 0; i < wl_size; i++) {
        wl_params.keys[i] = trace.at(i).key_hash;
        wl_params.op[i] = trace.at(i).op;
    }

    trace.clear();
    trace.shrink_to_fit();

    return ret;

err_free_ops:
    munmap_helper(wl_params.keys, wl_size * sizeof(*(wl_params.keys)));
err:
    return -1;
}

