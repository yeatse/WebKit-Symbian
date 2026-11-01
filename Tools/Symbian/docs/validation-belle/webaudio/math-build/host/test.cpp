#include <gtest/gtest.h>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

extern "C" float owned_log2f(float);

TEST(Log2fContract, FixedFiniteFloatBitPatterns) {
  std::uint32_t checks = 0;
  for (std::uint32_t exponent = 0; exponent < 255; ++exponent) {
    for (std::uint32_t k = 0; k < 257; ++k) {
      const std::uint32_t mantissa = (k * 0x7fffffULL) / 256;
      const float input = std::bit_cast<float>((exponent << 23) | mantissa);
      if (input == 0) continue;
      const float reference = std::log2(input);
      const float actual = owned_log2f(input);
      const float next = std::nextafter(reference, std::numeric_limits<float>::infinity());
      const float prev = std::nextafter(reference, -std::numeric_limits<float>::infinity());
      EXPECT_GE(actual, prev) << input;
      EXPECT_LE(actual, next) << input;
      ++checks;
    }
  }
  EXPECT_EQ(checks, 65534U);
}

TEST(Log2fContract, StandardDomainAndSpecialValues) {
  EXPECT_EQ(owned_log2f(1.f), 0.f);
  EXPECT_FALSE(std::signbit(owned_log2f(1.f)));
  EXPECT_EQ(owned_log2f(2.f), 1.f);
  EXPECT_EQ(owned_log2f(.5f), -1.f);
  EXPECT_EQ(owned_log2f(0.f), -std::numeric_limits<float>::infinity());
  EXPECT_EQ(owned_log2f(-0.f), -std::numeric_limits<float>::infinity());
  EXPECT_EQ(owned_log2f(std::numeric_limits<float>::infinity()), std::numeric_limits<float>::infinity());
  EXPECT_TRUE(std::isnan(owned_log2f(-1.f)));
  EXPECT_TRUE(std::isnan(owned_log2f(-std::numeric_limits<float>::infinity())));
  EXPECT_TRUE(std::isnan(owned_log2f(std::numeric_limits<float>::quiet_NaN())));
}
