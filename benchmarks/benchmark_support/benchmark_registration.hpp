#pragma once

// Loop-safe benchmark registration. The stock BENCHMARK() macro declares a
// static variable and therefore registers only the first loop iteration; the
// Phase-6 dimension matrices register inside loops, so they use this helper
// which calls RegisterBenchmarkInternal directly.

#include <benchmark/benchmark.h>

#include <memory>
#include <string>

namespace bmd_projection::m5::benchmark {

inline ::benchmark::Benchmark* register_benchmark(std::string name,
                                                  ::benchmark::internal::Function* function) {
    return ::benchmark::internal::RegisterBenchmarkInternal(
        std::make_unique<::benchmark::internal::FunctionBenchmark>(std::move(name), function));
}

} // namespace bmd_projection::m5::benchmark

#define BMD_PHASE6_REGISTER(name, fn)                                                              \
    ::bmd_projection::m5::benchmark::register_benchmark(                                           \
        ::std::string{#name}, static_cast<::benchmark::internal::Function*>(fn))
