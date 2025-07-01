#include <benchmark/benchmark.h>

#include <string>
#include <cstring>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <utility>

// Standard std::string has a static internal storage for small strings (15)
// and allocates dynamic only when the length exceeds the static storage
//
// TODO: Why multithreaded access doesn't increase the cases where dynamic allocation is used
void BM_LocalString(benchmark::State& state) {
    std::string src(state.range(0), '.');

    assert(std::strlen(src.data()) == state.range(0));

    // The size is not just pointer + size_t, it has static storage
    assert(sizeof(std::string) == 32);
    assert(sizeof(std::string) > sizeof(std::vector<char>));

    for (auto _ : state) {
        std::string str = src.data();
    }
}
BENCHMARK(BM_LocalString)->Arg(10)->Arg(15)->Arg(16)->Arg(17)->Arg(32)->Threads(1)->Threads(4);


// extract() may be used to extract nodes from a associative container
// and move to another or change the key value (and insert back to the same container)
void BM_ChangeMapKey_RemoveInsert(benchmark::State& state) {
    std::unordered_map<int, std::pair<int, int>> map;

    for (auto _ : state) {
        state.PauseTiming();
        for(size_t i = 0; i < state.range(0); ++i) {
            map[i * 10] = {i, i + 10};
        }
        state.ResumeTiming();

        for(size_t i = 0; i < state.range(0); ++i) {
            auto value = map[i * 10];
            map.erase(i * 10);
            map[i * 10 + 1] = value;
        }
    }
}
BENCHMARK(BM_ChangeMapKey_RemoveInsert)->Arg(1000);

void BM_ChangeMapKey_ExtractNode(benchmark::State& state) {
    std::unordered_map<int, std::pair<int, int>> map;

    for (auto _ : state) {
        state.PauseTiming();
        map.clear();
        for(size_t i = 0; i < state.range(0); ++i) {
            map[i * 10] = {i, i + 10};
        }
        state.ResumeTiming();

        for(size_t i = 0; i < state.range(0); ++i) {
            auto node = map.extract(i * 10);
            ++node.key();
            map.insert(std::move(node));
        }
    }
}
BENCHMARK(BM_ChangeMapKey_ExtractNode)->Arg(1000);

struct TData {
    long long A;
    char B;
    double C;
    int D[12];
};

struct TTrivial {
    // Just to make Trivial equal in size compared to Virtual
    void* FakeVMT;
    TData Data;
};

struct TVirtual {
    TData Data;
    virtual ~TVirtual() = default;
};

static_assert(sizeof(TTrivial) == sizeof(TVirtual));
static_assert(std::is_trivial_v<TTrivial> && std::is_trivially_copyable_v<TTrivial>);
static_assert(!std::is_trivial_v<TVirtual> && !std::is_trivially_copyable_v<TVirtual>);

// Test explanation:
// vector copy constructor uses __uninitialized_copy which uses
// std::copy to manipulate actual elements data. std::copy uses memcpy/memmove for trivial objects
// which is supposed to be faster
template<typename T>
void BM_VectorCopy(benchmark::State& state) {
    std::vector<T> src(state.range(0));
    for (auto _ : state) {
        std::vector<T> v;
        benchmark::DoNotOptimize(v = src);
    }
}
BENCHMARK(BM_VectorCopy<TTrivial>)->Arg(1000);
BENCHMARK(BM_VectorCopy<TVirtual>)->Arg(1000);

template<typename T>
void BM_VectorPushBack(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<T> v;
        while(v.size() < state.range(0)) {
            v.emplace_back();
        }
    }
}
BENCHMARK(BM_VectorPushBack<TTrivial>)->Arg(1000000);
BENCHMARK(BM_VectorPushBack<TVirtual>)->Arg(1000000);
