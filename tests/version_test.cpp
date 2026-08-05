#include <binance_market_data/projection/v1/version.hpp>

#include <gtest/gtest.h>

#include <string_view>
#include <type_traits>

namespace projection = binance_market_data::projection::v1;

TEST(VersionTest, ExposesProjectIdentity) {
    EXPECT_EQ(projection::kProjectName, "BinanceMarketDataProjection");
    EXPECT_EQ(projection::kProjectVersion, "0.1.0-alpha.0");
}

TEST(VersionTest, LibraryVersionMatchesStableProjectVersion) {
    static_assert(noexcept(projection::library_version()));
    static_assert(std::is_same_v<decltype(projection::library_version()), std::string_view>);

    const std::string_view first = projection::library_version();
    const std::string_view second = projection::library_version();

    EXPECT_FALSE(first.empty());
    EXPECT_EQ(first, projection::kProjectVersion);
    EXPECT_EQ(first.data(), second.data());
}
