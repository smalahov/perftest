#include <benchmark/benchmark.h>

#include <iostream>
#include <mutex>
#include <thread>

template<typename TLimiter, typename TFunction>
struct TLimitedExecuter {

    TLimitedExecuter(TFunction& function, size_t runLimit):
        Function(function),
        Limiter(runLimit) {}

    template<typename... Args>
    inline bool Run(Args&&... args) {
        bool doRun = Limiter.DoRun();
        {
        }

        if(doRun) {
            Function(args...);
        }

        return doRun;
    }

    inline void Reset() {
        Limiter.Reset();
    }

private:
    TLimiter Limiter;
    TFunction& Function;
};

struct TMutexLimiter {

    TMutexLimiter(size_t runLimit):
        RunLimit(runLimit),
        RunCount(0) {}

    inline bool DoRun() {
        std::lock_guard<std::mutex> g(Mutex);
        bool doRun = RunCount < RunLimit;
        if(doRun) {
            ++RunCount;
        }

        return doRun;
    }

    inline void Reset() {
        std::lock_guard<std::mutex> g(Mutex);
        RunCount = 0;
    }

private:
    size_t RunCount;
    size_t RunLimit;
    std::mutex Mutex;
};

struct TAtomicLimiter {

    TAtomicLimiter(size_t runLimit):
        RunLimit(runLimit),
        RunCount(0)
    {
        // We are interested in pure lock-free only
        assert(RunCount.is_lock_free());
    }

    inline bool DoRun() {
        if(RunCount.load(std::memory_order_relaxed) >= RunLimit) {
            return false;
        }

        return RunCount.fetch_add(1, std::memory_order_relaxed) < RunLimit;
    }

    inline void Reset() {
        RunCount = 0;
    }

private:
    std::atomic<size_t> RunCount;
    size_t RunLimit;
};

template<bool Atomic>
struct TCounter {};

template<>
struct TCounter<true> {
    using Type = std::atomic<size_t>;

    static inline void Inc(Type& v) {
        v.fetch_add(1, std::memory_order_relaxed);
    }
};

// Volatile counter type is here just to keep called function from being optimized out
// I fact it doesnt count anything, just produces some instructions
template<>
struct TCounter<false> {
    using Type = volatile size_t;

    static inline void Inc(Type& v) {
       v = 1;
    }
};

// Uses TLimitedExecuter with TLimiter as limiting mechanism to run a function
// exactly N times in several threads
// Compares TMutexLimiter and TAtomicLimiter
template<typename TLimiter, bool DoVerify>
void BM_LimitedRun(benchmark::State& state) {
    size_t limit = state.range(0);
    size_t threadsCount = state.range(1);
    std::vector<std::thread> threads;

    using TCounterType = TCounter<DoVerify>;
    typename TCounterType::Type counter;

    auto function = [&counter](){
        TCounterType::Inc(counter);
    };

    for (auto _ : state) {
        counter = 0;
        TLimitedExecuter<TLimiter, decltype(function)> executer(function, limit);

        for(size_t i = 0; i < threadsCount; ++i) {
            threads.emplace_back([&executer](){
                while(executer.Run());
            });
        }
        for_each(threads.begin(), threads.end(), [](auto& t) { t.join(); });
        threads.clear();

        assert(!DoVerify || counter == limit);
    }
}
BENCHMARK(BM_LimitedRun<TMutexLimiter, true>)->Args({10'000, 2});
BENCHMARK(BM_LimitedRun<TMutexLimiter, false>)->Args({10'000, 2});
BENCHMARK(BM_LimitedRun<TAtomicLimiter, true>)->Args({10'000, 2});
BENCHMARK(BM_LimitedRun<TAtomicLimiter, false>)->Args({10'000, 2});
