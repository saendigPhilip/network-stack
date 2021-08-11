#include <vector>
#include "generate_traces.h"

static constexpr size_t wl_size = 10000000;
constexpr size_t total_ops{wl_size / 5};
static constexpr size_t key_size = sizeof(uint64_t);


struct workload_params {
    uint64_t *keys;
    anchor_tr_::Trace_cmd::operation *op;
};
extern struct workload_params wl_params;


int workload_scan();

void release_workload();

