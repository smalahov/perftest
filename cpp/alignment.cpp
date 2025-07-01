#include <benchmark/benchmark.h>
#include <sched.h>
#include <unistd.h>

void BM_MisalignmentAsm(benchmark::State& state) {
    alignas(long long) unsigned char data[102400];
    const size_t offset = state.range(0);

    cpu_set_t oldSet;
    assert(sched_getaffinity(getpid(), sizeof(oldSet), &oldSet) == 0);

    // Assign to a single performance core
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    assert(sched_setaffinity(getpid(), sizeof(set), &set) == 0);

    assert(offset < sizeof(long long));

    //TODO: Explain: without leading nop's the difference in alignment is not visible
    for (auto _ : state) {
        __asm__ volatile (
            "nop;"
            "nop;"
            "nop;"
            "nop;"
            "nop;"
            "mov $10000, %%rax;"
            "lea %0, %%rbx;"
            "add %1, %%rbx;"
            "mov $0x1, %%rcx;"
            "loop:"

            "movq $01, (%%rbx);"
            "add $8, %%rbx;"
            "movq %%rbx, (%%rbx);"

            "add $-1, %%rax;"
            "jne loop;"
            :
            : "m" (data), "m" (offset)
            : "rax", "rbx", "rcx", "rdx"
        );
    }

    assert(sched_setaffinity(getpid(), sizeof(set), &oldSet) == 0);
}
BENCHMARK(BM_MisalignmentAsm)->DenseRange(0, 4);


template<typename T>
void BM_Misalignment(benchmark::State& state) {
    size_t offset = state.range(0);

    static_assert(alignof(T) > 1);
    assert(offset < sizeof(T));

    constexpr size_t Count = 10240;
    alignas(T) unsigned char data[(Count + 1)*sizeof(T)];
    for (auto _ : state) {
        T* ptr = reinterpret_cast<T*>(&data[offset]);

        assert(!offset || reinterpret_cast<long int>(ptr) % alignof(T));

        //TODO: Explain: without volatile here the difference is not visible
        T* volatile  prev = nullptr;
        for(size_t i = 0; i < Count; ++i) {
            *ptr += prev ? *prev : 0;
            prev = ptr;
            ++ptr;
        }
    }
}
BENCHMARK(BM_Misalignment<long long>)->Arg(0)->Arg(1)->Arg(0);
BENCHMARK(BM_Misalignment<double>)->Arg(0)->Arg(1)->Arg(2);
