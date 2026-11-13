#include "pch.h"
#include "TestFixture.h"

// ============================================================================
// Rationalize Function Tests
// ============================================================================

TEST_F(ZombieSchemeTest, RationalizeBasic) {
    // Test basic rationalize with pi approximation
    Object* result = eval("(rationalize 3.14159 0.01)");
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(numeric::is_number(result));

    double value = numeric::to_double(result);
    // Should be close to 22/7 = 3.142857...
    EXPECT_NEAR(value, 22.0/7.0, 0.01);
}

TEST_F(ZombieSchemeTest, RationalizeExact) {
    // Test with zero tolerance - should return exact value
    Object* result = eval("(rationalize 0.5 0)");
    ASSERT_NE(result, nullptr);
    EXPECT_NEAR(numeric::to_double(result), 0.5, 0.0001);
}

TEST_F(ZombieSchemeTest, RationalizeLargeTolerance) {
    // Test with large tolerance - should simplify to simplest value
    Object* result = eval("(rationalize 1.5 0.5)");
    ASSERT_NE(result, nullptr);

    double value = numeric::to_double(result);
    // Range is [1.0, 2.0], simplest is 1
    EXPECT_TRUE(value >= 1.0 && value <= 2.0);
}

TEST_F(ZombieSchemeTest, RationalizeNegativeTolerance) {
    // Test with negative tolerance - should use absolute value
    Object* result = eval("(rationalize 2.7 -0.1)");
    ASSERT_NE(result, nullptr);

    double value = numeric::to_double(result);
    // Should be in range [2.6, 2.8]
    EXPECT_TRUE(value >= 2.6 && value <= 2.8);
}

TEST_F(ZombieSchemeTest, RationalizeSmallTolerance) {
    // Test with very small tolerance
    Object* result = eval("(rationalize 0.333333 0.0001)");
    ASSERT_NE(result, nullptr);

    double value = numeric::to_double(result);
    // Should be very close to 1/3
    EXPECT_NEAR(value, 1.0/3.0, 0.0001);
}

TEST_F(ZombieSchemeTest, RationalizeInteger) {
    // Test with integer input
    Object* result = eval("(rationalize 5 0.1)");
    ASSERT_NE(result, nullptr);

    double value = numeric::to_double(result);
    // Should stay as 5
    EXPECT_NEAR(value, 5.0, 0.1);
}

TEST_F(ZombieSchemeTest, RationalizeThirds) {
    // Test approximating 1/3
    Object* result = eval("(rationalize 0.3333 0.001)");
    ASSERT_NE(result, nullptr);

    double value = numeric::to_double(result);
    // Should be close to 1/3 = 0.333333...
    EXPECT_NEAR(value, 1.0/3.0, 0.001);
}

TEST_F(ZombieSchemeTest, RationalizeGoldenRatio) {
    // Test approximating golden ratio φ ≈ 1.618
    Object* result = eval("(rationalize 1.618 0.01)");
    ASSERT_NE(result, nullptr);

    double value = numeric::to_double(result);
    // Common approximations: 8/5=1.6, 13/8=1.625, 21/13=1.615...
    EXPECT_TRUE(value >= 1.608 && value <= 1.628);
}
