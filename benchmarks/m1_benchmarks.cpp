#include "benchmark_support/workload_spec.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_format.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>
#include <binance_market_data/projection/v1/numeric/price_units.hpp>
#include <binance_market_data/projection/v1/numeric/quantity_units.hpp>

#include <benchmark/benchmark.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace {

namespace core = binance_market_data::projection::v1;
namespace bm = bmd_projection::m5::benchmark;

// Homogeneous batched logical-operation block: the framework overhead stays
// subordinate to the normative public operation, which is unchanged.
constexpr std::size_t kOperationBatch = 16;

[[nodiscard]] core::DecimalScale scale_8() {
    const auto scale = core::DecimalScale::create(8);
    if (!scale.has_value()) {
        std::abort();
    }
    return *scale;
}

[[nodiscard]] core::PriceUnits units_123456789() {
    const auto value = core::PriceUnits::create(123'456'789);
    if (!value.has_value()) {
        std::abort();
    }
    return *value;
}

[[nodiscard]] core::QuantityUnits units_zero() {
    const auto value = core::QuantityUnits::create(0);
    if (!value.has_value()) {
        std::abort();
    }
    return *value;
}

[[nodiscard]] std::array<std::string_view, kOperationBatch> batch_of(std::string_view input) {
    std::array<std::string_view, kOperationBatch> inputs{};
    inputs.fill(input);
    return inputs;
}

// ---------------------------------------------------------------------------
// Static workload-spec registration (OD-M5-P6-003 / OD-M5-P6-036). The
// registry is complete regardless of the active filter.
// ---------------------------------------------------------------------------
namespace {

struct M1CaseSpec final {
    const char* name;
    const char* operation;
    const char* input;
    const char* expected_disposition;
    const char* expected_units;
    const char* expected_output;
};

constexpr M1CaseSpec kM1Cases[] = {
    {"M1/ParsePrice/MatchedScale", "parse_price", "1.23456789", "success", "123456789", "-"},
    {"M1/ParsePositiveQuantity/MatchedScale", "parse_positive_quantity", "1.23456789", "success",
     "-", "-"},
    {"M1/ParseQuantity/ZeroSuccess", "parse_quantity", "0", "success", "0", "-"},
    {"M1/ParsePositiveQuantity/ZeroRejected", "parse_positive_quantity", "0", "ZeroNotAllowed", "-",
     "-"},
    {"M1/ParsePrice/ExactUpscale", "parse_price", "1.2345", "success", "123450000", "-"},
    {"M1/ParsePrice/ExactDownscale", "parse_price", "1.234567890000000000", "success", "123456789",
     "-"},
    {"M1/ParsePrice/InexactDownscaleRejected", "parse_price", "1.234567890123456789",
     "InexactScale", "-", "-"},
    {"M1/ParsePrice/OverflowRejected", "parse_price", "92233720368.54775808", "Overflow", "-", "-"},
    {"M1/ParsePrice/SyntaxRejected", "parse_price", "1e3", "InvalidSyntax", "-", "-"},
    {"M1/FormatPriceFixed", "format_price_fixed", "units=123456789 scale=8", "success", "-",
     "1.23456789"},
    {"M1/FormatQuantityFixed", "format_quantity_fixed", "units=0 scale=8", "success", "-",
     "0.00000000"},
};

const auto kM1SpecRegistration = [] {
    for (const auto& spec : kM1Cases) {
        auto& builder = bm::register_workload(spec.name);
        builder.set("benchmark_name", spec.name);
        builder.set("operation", spec.operation);
        builder.set("input", spec.input);
        builder.set("storage_scale", "8");
        builder.set("expected_disposition", spec.expected_disposition);
        builder.set("expected_units", spec.expected_units);
        builder.set("expected_output", spec.expected_output);
        builder.set("operation_batch", kOperationBatch);
        builder.set("primary_timer", "cpu");
        builder.set("primary_denominator", "cpu_time");
        builder.set("generator_schema", "M5_PHASE6_M1_FIXED_CASES_V1");
    }
    return 0;
}();

} // namespace

// ---------------------------------------------------------------------------
// Semantics: the normative M1 input/result table is validated once per
// benchmark invocation, entirely outside the measured region. Any deviation
// fails the benchmark closed via SkipWithError.
// ---------------------------------------------------------------------------

static void BM_ParsePriceMatchedScale(benchmark::State& state) {
    constexpr std::string_view input = "1.23456789";
    const auto scale = scale_8();
    {
        const auto preflight = core::parse_price(input, scale);
        if (!std::holds_alternative<core::ParsedDecimal<core::PriceUnits>>(preflight) ||
            std::get<core::ParsedDecimal<core::PriceUnits>>(preflight).value.value() !=
                123'456'789) {
            state.SkipWithError("M1/ParsePrice/MatchedScale semantic precondition failed");
            return;
        }
    }
    const auto inputs = batch_of(input);
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        for (const auto text : inputs) {
            const auto result = core::parse_price(text, scale);
            if (std::holds_alternative<core::ParsedDecimal<core::PriceUnits>>(result)) {
                const auto& parsed = std::get<core::ParsedDecimal<core::PriceUnits>>(result);
                accumulator += static_cast<std::uint64_t>(parsed.value.value());
                accumulator += parsed.source_fraction_digits;
            } else {
                accumulator += 0xDEAD;
            }
        }
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(static_cast<std::int64_t>(kOperationBatch));
    }
}

static void BM_ParsePositiveQuantityMatchedScale(benchmark::State& state) {
    constexpr std::string_view input = "1.23456789";
    const auto scale = scale_8();
    {
        const auto preflight = core::parse_positive_quantity(input, scale);
        if (!std::holds_alternative<core::ParsedDecimal<core::QuantityUnits>>(preflight)) {
            state.SkipWithError("M1/ParsePositiveQuantity/MatchedScale semantic precondition "
                                "failed");
            return;
        }
    }
    const auto inputs = batch_of(input);
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        for (const auto text : inputs) {
            const auto result = core::parse_positive_quantity(text, scale);
            if (std::holds_alternative<core::ParsedDecimal<core::QuantityUnits>>(result)) {
                const auto& parsed = std::get<core::ParsedDecimal<core::QuantityUnits>>(result);
                accumulator += static_cast<std::uint64_t>(parsed.value.value());
                accumulator += parsed.source_fraction_digits;
            } else {
                accumulator += 0xDEAD;
            }
        }
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(static_cast<std::int64_t>(kOperationBatch));
    }
}

static void BM_ParseQuantityZeroSuccess(benchmark::State& state) {
    constexpr std::string_view input = "0";
    const auto scale = scale_8();
    {
        const auto preflight = core::parse_quantity(input, scale);
        if (!std::holds_alternative<core::ParsedDecimal<core::QuantityUnits>>(preflight) ||
            std::get<core::ParsedDecimal<core::QuantityUnits>>(preflight).value.value() != 0) {
            state.SkipWithError("M1/ParseQuantity/ZeroSuccess semantic precondition failed");
            return;
        }
    }
    const auto inputs = batch_of(input);
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        for (const auto text : inputs) {
            const auto result = core::parse_quantity(text, scale);
            if (std::holds_alternative<core::ParsedDecimal<core::QuantityUnits>>(result)) {
                const auto& parsed = std::get<core::ParsedDecimal<core::QuantityUnits>>(result);
                accumulator += static_cast<std::uint64_t>(parsed.value.value());
                accumulator += parsed.source_fraction_digits;
            } else {
                accumulator += 0xDEAD;
            }
        }
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(static_cast<std::int64_t>(kOperationBatch));
    }
}

static void BM_ParsePositiveQuantityZeroRejected(benchmark::State& state) {
    constexpr std::string_view input = "0";
    const auto scale = scale_8();
    {
        const auto preflight = core::parse_positive_quantity(input, scale);
        if (!std::holds_alternative<core::DecimalError>(preflight) ||
            std::get<core::DecimalError>(preflight).code !=
                core::DecimalErrorCode::ZeroNotAllowed) {
            state.SkipWithError("M1/ParsePositiveQuantity/ZeroRejected semantic precondition "
                                "failed");
            return;
        }
    }
    const auto inputs = batch_of(input);
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        for (const auto text : inputs) {
            const auto result = core::parse_positive_quantity(text, scale);
            if (std::holds_alternative<core::DecimalError>(result)) {
                accumulator += static_cast<std::uint64_t>(
                    static_cast<std::uint8_t>(std::get<core::DecimalError>(result).code));
            } else {
                accumulator += 0xDEAD;
            }
        }
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(static_cast<std::int64_t>(kOperationBatch));
    }
}

static void BM_ParsePriceExactUpscale(benchmark::State& state) {
    constexpr std::string_view input = "1.2345";
    const auto scale = scale_8();
    {
        const auto preflight = core::parse_price(input, scale);
        if (!std::holds_alternative<core::ParsedDecimal<core::PriceUnits>>(preflight) ||
            std::get<core::ParsedDecimal<core::PriceUnits>>(preflight).value.value() !=
                123'450'000) {
            state.SkipWithError("M1/ParsePrice/ExactUpscale semantic precondition failed");
            return;
        }
    }
    const auto inputs = batch_of(input);
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        for (const auto text : inputs) {
            const auto result = core::parse_price(text, scale);
            if (std::holds_alternative<core::ParsedDecimal<core::PriceUnits>>(result)) {
                accumulator += static_cast<std::uint64_t>(
                    std::get<core::ParsedDecimal<core::PriceUnits>>(result).value.value());
            } else {
                accumulator += 0xDEAD;
            }
        }
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(static_cast<std::int64_t>(kOperationBatch));
    }
}

static void BM_ParsePriceExactDownscale(benchmark::State& state) {
    constexpr std::string_view input = "1.234567890000000000";
    const auto scale = scale_8();
    {
        const auto preflight = core::parse_price(input, scale);
        if (!std::holds_alternative<core::ParsedDecimal<core::PriceUnits>>(preflight) ||
            std::get<core::ParsedDecimal<core::PriceUnits>>(preflight).value.value() !=
                123'456'789) {
            state.SkipWithError("M1/ParsePrice/ExactDownscale semantic precondition failed");
            return;
        }
    }
    const auto inputs = batch_of(input);
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        for (const auto text : inputs) {
            const auto result = core::parse_price(text, scale);
            if (std::holds_alternative<core::ParsedDecimal<core::PriceUnits>>(result)) {
                accumulator += static_cast<std::uint64_t>(
                    std::get<core::ParsedDecimal<core::PriceUnits>>(result).value.value());
            } else {
                accumulator += 0xDEAD;
            }
        }
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(static_cast<std::int64_t>(kOperationBatch));
    }
}

static void BM_ParsePriceInexactDownscaleRejected(benchmark::State& state) {
    constexpr std::string_view input = "1.234567890123456789";
    const auto scale = scale_8();
    {
        const auto preflight = core::parse_price(input, scale);
        if (!std::holds_alternative<core::DecimalError>(preflight) ||
            std::get<core::DecimalError>(preflight).code != core::DecimalErrorCode::InexactScale) {
            state.SkipWithError("M1/ParsePrice/InexactDownscaleRejected semantic precondition "
                                "failed");
            return;
        }
    }
    const auto inputs = batch_of(input);
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        for (const auto text : inputs) {
            const auto result = core::parse_price(text, scale);
            if (std::holds_alternative<core::DecimalError>(result)) {
                accumulator += static_cast<std::uint64_t>(
                    static_cast<std::uint8_t>(std::get<core::DecimalError>(result).code));
            } else {
                accumulator += 0xDEAD;
            }
        }
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(static_cast<std::int64_t>(kOperationBatch));
    }
}

static void BM_ParsePriceOverflowRejected(benchmark::State& state) {
    constexpr std::string_view input = "92233720368.54775808";
    const auto scale = scale_8();
    {
        const auto preflight = core::parse_price(input, scale);
        if (!std::holds_alternative<core::DecimalError>(preflight) ||
            std::get<core::DecimalError>(preflight).code != core::DecimalErrorCode::Overflow) {
            state.SkipWithError("M1/ParsePrice/OverflowRejected semantic precondition failed");
            return;
        }
    }
    const auto inputs = batch_of(input);
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        for (const auto text : inputs) {
            const auto result = core::parse_price(text, scale);
            if (std::holds_alternative<core::DecimalError>(result)) {
                accumulator += static_cast<std::uint64_t>(
                    static_cast<std::uint8_t>(std::get<core::DecimalError>(result).code));
            } else {
                accumulator += 0xDEAD;
            }
        }
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(static_cast<std::int64_t>(kOperationBatch));
    }
}

static void BM_ParsePriceSyntaxRejected(benchmark::State& state) {
    constexpr std::string_view input = "1e3";
    const auto scale = scale_8();
    {
        const auto preflight = core::parse_price(input, scale);
        if (!std::holds_alternative<core::DecimalError>(preflight) ||
            std::get<core::DecimalError>(preflight).code != core::DecimalErrorCode::InvalidSyntax) {
            state.SkipWithError("M1/ParsePrice/SyntaxRejected semantic precondition failed");
            return;
        }
    }
    const auto inputs = batch_of(input);
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        for (const auto text : inputs) {
            const auto result = core::parse_price(text, scale);
            if (std::holds_alternative<core::DecimalError>(result)) {
                accumulator += static_cast<std::uint64_t>(
                    static_cast<std::uint8_t>(std::get<core::DecimalError>(result).code));
            } else {
                accumulator += 0xDEAD;
            }
        }
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(static_cast<std::int64_t>(kOperationBatch));
    }
}

template <typename Units, typename Formatter>
void run_format_benchmark(benchmark::State& state, Units value, Formatter formatter,
                          std::string_view expected, const char* name) {
    const auto scale = scale_8();
    {
        const auto preflight = formatter(value, scale);
        if (!std::holds_alternative<std::string>(preflight) ||
            std::get<std::string>(preflight) != expected) {
            state.SkipWithError(std::string{name} + " semantic precondition failed");
            return;
        }
    }
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        for (std::size_t index = 0; index < kOperationBatch; ++index) {
            const auto result = formatter(value, scale);
            if (std::holds_alternative<std::string>(result)) {
                const auto& text = std::get<std::string>(result);
                accumulator += text.size();
                if (!text.empty()) {
                    accumulator += static_cast<unsigned char>(text.front());
                }
            } else {
                accumulator += 0xDEAD;
            }
        }
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(static_cast<std::int64_t>(kOperationBatch));
    }
}

static void BM_FormatPriceFixed(benchmark::State& state) {
    run_format_benchmark(state, units_123456789(), core::format_price_fixed, "1.23456789",
                         "M1/FormatPriceFixed");
}

static void BM_FormatQuantityFixed(benchmark::State& state) {
    run_format_benchmark(state, units_zero(), core::format_quantity_fixed, "0.00000000",
                         "M1/FormatQuantityFixed");
}

} // namespace

BENCHMARK(BM_ParsePriceMatchedScale)->Name("M1/ParsePrice/MatchedScale")->MinTime(0.05);
BENCHMARK(BM_ParsePositiveQuantityMatchedScale)
    ->Name("M1/ParsePositiveQuantity/MatchedScale")
    ->MinTime(0.05);
BENCHMARK(BM_ParseQuantityZeroSuccess)->Name("M1/ParseQuantity/ZeroSuccess")->MinTime(0.05);
BENCHMARK(BM_ParsePositiveQuantityZeroRejected)
    ->Name("M1/ParsePositiveQuantity/ZeroRejected")
    ->MinTime(0.05);
BENCHMARK(BM_ParsePriceExactUpscale)->Name("M1/ParsePrice/ExactUpscale")->MinTime(0.05);
BENCHMARK(BM_ParsePriceExactDownscale)->Name("M1/ParsePrice/ExactDownscale")->MinTime(0.05);
BENCHMARK(BM_ParsePriceInexactDownscaleRejected)
    ->Name("M1/ParsePrice/InexactDownscaleRejected")
    ->MinTime(0.05);
BENCHMARK(BM_ParsePriceOverflowRejected)->Name("M1/ParsePrice/OverflowRejected")->MinTime(0.05);
BENCHMARK(BM_ParsePriceSyntaxRejected)->Name("M1/ParsePrice/SyntaxRejected")->MinTime(0.05);
BENCHMARK(BM_FormatPriceFixed)->Name("M1/FormatPriceFixed")->MinTime(0.05);
BENCHMARK(BM_FormatQuantityFixed)->Name("M1/FormatQuantityFixed")->MinTime(0.05);
