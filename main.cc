#include <bitwuzla.h>
#include <stdlib.h>
#include <time.h>
#include <vector>
#include <assert.h>

#include <chrono>
using namespace std::chrono;

Bitwuzla* bzla;

constexpr unsigned flog2(unsigned x) { return x <= 1 ? 0 : 1 + flog2(x >> 1); }
constexpr unsigned clog2(unsigned x) { return x <= 1 ? 0 : flog2(x - 1) + 1; }

template <unsigned size, unsigned width>
struct SymMemory {
    const BitwuzlaTerm* sym;

    const BitwuzlaSort* sort_idx;
    const BitwuzlaSort* sort_val;

    SymMemory() {
        sort_idx = bitwuzla_mk_bv_sort(bzla, clog2(size));
        sort_val = bitwuzla_mk_bv_sort(bzla, width);
        const BitwuzlaSort* array = bitwuzla_mk_array_sort(bzla, sort_idx, sort_val);
        sym = bitwuzla_mk_const(bzla, array, "array");
        const BitwuzlaTerm* init_term = bitwuzla_mk_bv_zero(bzla, sort_val);
        for (unsigned i = 0; i < size; i++) {
            const BitwuzlaTerm* i_term = bitwuzla_mk_bv_value_uint64(bzla, sort_idx, i);
            sym = bitwuzla_mk_term3(bzla, BITWUZLA_KIND_ARRAY_STORE, sym, i_term, init_term);
        }
    }

    const BitwuzlaTerm* read(const BitwuzlaTerm* idx) {
        return bitwuzla_mk_term2(bzla, BITWUZLA_KIND_ARRAY_SELECT, sym, idx);
    }

    void write(const BitwuzlaTerm* idx, const BitwuzlaTerm* value) {
        sym = bitwuzla_mk_term3(bzla, BITWUZLA_KIND_ARRAY_STORE, sym, idx, value);
    }
};

template <unsigned size, unsigned width>
struct ConcMemory {
    uint64_t vals[size];

    ConcMemory() {
        for (unsigned i = 0; i < size; i++) {
            vals[i] = 0;
        }
    }

    uint64_t read(uint64_t idx) {
        return vals[idx];
    }

    void write(uint64_t idx, uint64_t value) {
        vals[idx] = value;
    }
};

const int seed = 42;

int main() {
    srand(seed);

    bzla = bitwuzla_new();
    bitwuzla_set_option(bzla, BITWUZLA_OPT_PRODUCE_MODELS, 1);
    bitwuzla_set_option(bzla, BITWUZLA_OPT_OUTPUT_NUMBER_FORMAT, BITWUZLA_BV_BASE_DEC);
    bitwuzla_set_option(bzla, BITWUZLA_OPT_INCREMENTAL, 1);

    constexpr int size = 128;
    const int n = 1000000;

    std::vector<const BitwuzlaTerm*> terms;
    SymMemory<size, 32> sym_mem;

    auto sym_start = high_resolution_clock::now();
    for (int i = 0; i < n; i++) {
        bool store = rand() & 1;
        uint32_t idx = rand() % size;
        if (store) {
            uint32_t val = rand();
            sym_mem.write(bitwuzla_mk_bv_value_uint64(bzla, sym_mem.sort_idx, idx), bitwuzla_mk_bv_value_uint64(bzla, sym_mem.sort_val, val));
        } else {
            terms.push_back(sym_mem.read(bitwuzla_mk_bv_value_uint64(bzla, sym_mem.sort_idx, idx)));
        }
    }
    bitwuzla_check_sat(bzla);
    auto sym_stop = high_resolution_clock::now();
    auto sym_duration = duration_cast<microseconds>(sym_stop - sym_start);

    srand(seed);

    ConcMemory<size, 32> conc_mem;

    std::vector<uint32_t> conc_vals;

    auto conc_start = high_resolution_clock::now();
    for (int i = 0; i < n; i++) {
        bool store = rand() & 1;
        uint32_t idx = rand() % size;
        if (store) {
            uint32_t val = rand();
            conc_mem.write(idx, val);
        } else {
            conc_vals.push_back(conc_mem.read(idx));
        }
    }
    auto conc_stop = high_resolution_clock::now();
    auto conc_duration = duration_cast<microseconds>(conc_stop - conc_start);

    assert(terms.size() == conc_vals.size());
    for (unsigned i = 0; i < terms.size(); i++) {
        const BitwuzlaTerm* t = terms[i];
        uint32_t val = strtol(bitwuzla_get_bv_value(bzla, t), NULL, 2);
        assert(val == conc_vals[i]);
    }

    printf("sym duration: %ld us\n", sym_duration.count());
    printf("conc duration: %ld us\n", conc_duration.count());

    return 0;
}
