#include <benchmark/benchmark.h>

#include <array>
#include <cstring>
#include <atomic>
#include <iostream>

template<size_t Size>
void BM_CacheFriendlyLoop(benchmark::State& state) {
    std::array<std::array<char, Size>, Size> data;
    int sum = 0;

    for (auto _ : state) {
        for(size_t row = 0; row < Size; ++row) {
            for(size_t col = 0; col < Size - 1; ++col) {
                benchmark::DoNotOptimize(data[row][col] += data[row][col + 1]);
            }
        }
    }
}

template<size_t Size>
void BM_CacheFoeLoop(benchmark::State& state) {
    std::array<std::array<char, Size>, Size> data;
    int sum = 0;

    for (auto _ : state) {
        for(size_t col = 0; col < Size - 1; ++col) {
            for(size_t row = 0; row < Size; ++row) {
                benchmark::DoNotOptimize(data[row][col] += data[row][col + 1]);
            }
        }
    }
}

BENCHMARK(BM_CacheFriendlyLoop<1024>);
BENCHMARK(BM_CacheFoeLoop<1024>);


// False sharing example
// Threads may be a problem for each other when they access the same cache line making it Invalid thus requiring
// sync on every change
// The decision is to add some space between records to move per-thread memory on separate cache lines
template<typename TSpan>
class TFalseSharingFixture : public benchmark::Fixture {
public:
    struct TArrayData {
        // volatile here is to force the code to store changes immediately
        // thus making the effect of false sharing problem visible and measurable
        volatile int     Int;
        int     Int2;
        TSpan   Span;
    };

    constexpr static size_t MaxThreads = 16;
    constexpr static size_t LoopCount = 100'000;
    constexpr static size_t SpanSize = sizeof(TSpan);

    void SetUp(::benchmark::State& st) {
    }

    void TearDown(::benchmark::State& st) {
        for(int i = 0; i < st.threads(); ++i) {
            assert(Data[i].Int == LoopCount);
        }
    }

    std::array<TArrayData, MaxThreads> Data;
};

BENCHMARK_TEMPLATE_METHOD_F(TFalseSharingFixture, FalseSharingTest)(benchmark::State& st) {
    if constexpr(Base::SpanSize > 8) {
        this->Data[st.thread_index()].Span[0] = 1;
    }
    for (auto _ : st) {
        this->Data[st.thread_index()].Int = 0;
        for(int i = 0; i < this->LoopCount; ++i) {
            benchmark::DoNotOptimize(++this->Data[st.thread_index()].Int);
        }
    }
}

using TNoSpan = struct {};
using TSpan64 = char[64 - 8];

BENCHMARK_TEMPLATE_INSTANTIATE_F(TFalseSharingFixture, FalseSharingTest, TNoSpan)->Threads(1)->Threads(2)->Threads(4);
BENCHMARK_TEMPLATE_INSTANTIATE_F(TFalseSharingFixture, FalseSharingTest, TSpan64)->Threads(1)->Threads(2)->Threads(4);

//TODO: Memory fences
//TODO: memory_order_relaxed
