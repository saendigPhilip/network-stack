#include <vector>
#include "generate_traces.h"

static constexpr size_t wl_size = 10000000;
static constexpr size_t key_size = sizeof(uint64_t);


struct workload_params {
    uint64_t *keys;
    anchor_tr_::Trace_cmd::operation *op;
};
extern struct workload_params wl_params;


std::vector<anchor_tr_::Trace_cmd>
workload_trace_init(char *wl_file);

void release_workload();

int workload_scan();

