// test/types/test_configurations/radio_configuration_test.cpp
#include <gtest/gtest.h>

#include "types/configurations/radio_configuration.hpp"

namespace loramesher {
namespace test {

class RadioConfigTest : public ::testing::Test {
   protected:
    void SetUp() override {
        defaultConfig = RadioConfig::CreateDefaultSx1276();
    }

    RadioConfig defaultConfig;
};

TEST_F(RadioConfigTest, DefaultConstructorCreatesValidConfig) {
    EXPECT_TRUE(defaultConfig.IsValid());
    EXPECT_FLOAT_EQ(defaultConfig.getFrequency(), 869.900F);
    EXPECT_EQ(defaultConfig.getSpreadingFactor(), 7);
    EXPECT_FLOAT_EQ(defaultConfig.getBandwidth(), 125.0);
    EXPECT_EQ(defaultConfig.getCodingRate(), 5);
    EXPECT_EQ(defaultConfig.getPower(), 17);
}

TEST_F(RadioConfigTest, FrequencyValidation) {
    EXPECT_THROW(defaultConfig.setFrequency(100.0F), std::invalid_argument);
    EXPECT_THROW(defaultConfig.setFrequency(1100.0F), std::invalid_argument);
    EXPECT_NO_THROW(defaultConfig.setFrequency(868.0F));
}

TEST_F(RadioConfigTest, SpreadingFactorValidation) {
    EXPECT_THROW(defaultConfig.setSpreadingFactor(5), std::invalid_argument);
    EXPECT_THROW(defaultConfig.setSpreadingFactor(13), std::invalid_argument);
    EXPECT_NO_THROW(defaultConfig.setSpreadingFactor(7));
}

TEST_F(RadioConfigTest, CodingRateValidation) {
    EXPECT_THROW(defaultConfig.setCodingRate(4), std::invalid_argument);
    EXPECT_THROW(defaultConfig.setCodingRate(9), std::invalid_argument);
    EXPECT_NO_THROW(defaultConfig.setCodingRate(5));
}

TEST_F(RadioConfigTest, ValidationMessages) {
    EXPECT_THROW(
        RadioConfig config(RadioType::kSx1276, 100.0F, 5, -1.0F, 4, 25),
        std::invalid_argument);
}

// ===========================================================================
// RadioConfigCoverageTest — targets uncovered lines in radio_configuration.cpp
// ===========================================================================

class RadioConfigCoverageTest : public ::testing::Test {
   protected:
    RadioConfig cfg_;  // default-constructed SX1276 config
};

TEST_F(RadioConfigCoverageTest, CreateDefaultSx1278IsValid) {
    RadioConfig c = RadioConfig::CreateDefaultSx1278();
    EXPECT_TRUE(c.IsValid());
    EXPECT_EQ(c.getRadioTypeString(), "SX1278");
}

TEST_F(RadioConfigCoverageTest, CreateDefaultSx1262IsValid) {
    RadioConfig c = RadioConfig::CreateDefaultSx1262();
    EXPECT_TRUE(c.IsValid());
    EXPECT_EQ(c.getRadioTypeString(), "SX1262");
}

TEST_F(RadioConfigCoverageTest, GetRadioTypeStringAllValues) {
    RadioConfig sx1276 = RadioConfig::CreateDefaultSx1276();
    EXPECT_EQ(sx1276.getRadioTypeString(), "SX1276");

    RadioConfig mock(RadioType::kMockRadio, 869.9f, 7, 125.0f, 5, 17);
    EXPECT_EQ(mock.getRadioTypeString(), "MockRadio");
}

TEST_F(RadioConfigCoverageTest, SetterMethodsSucceedOnValidConfig) {
    RadioConfig c = RadioConfig::CreateDefaultSx1276();
    EXPECT_TRUE(c.setSyncWord(0x12).IsSuccess());
    EXPECT_TRUE(c.setCRC(true).IsSuccess());
    EXPECT_TRUE(c.setCRC(false).IsSuccess());
    EXPECT_TRUE(c.setPreambleLength(16).IsSuccess());
}

TEST_F(RadioConfigCoverageTest, SetBandwidthInvalidThrows) {
    RadioConfig c = RadioConfig::CreateDefaultSx1276();
    EXPECT_THROW(c.setBandwidth(-1.0f), std::invalid_argument);
    EXPECT_THROW(c.setBandwidth(0.0f), std::invalid_argument);
}

TEST_F(RadioConfigCoverageTest, SetBandwidthValid) {
    RadioConfig c = RadioConfig::CreateDefaultSx1276();
    EXPECT_NO_THROW(c.setBandwidth(250.0f));
}

TEST_F(RadioConfigCoverageTest, SetPowerValid) {
    RadioConfig c = RadioConfig::CreateDefaultSx1276();
    EXPECT_NO_THROW(c.setPower(14));
}

TEST_F(RadioConfigCoverageTest, SetPowerTooHighThrows) {
    RadioConfig c = RadioConfig::CreateDefaultSx1276();
    EXPECT_THROW(c.setPower(21), std::invalid_argument);
}

TEST_F(RadioConfigCoverageTest, ValidateMultipleErrors) {
    try {
        RadioConfig bad(RadioType::kSx1276, 100.0f, 5, -1.0f, 4, 25);
        FAIL() << "Expected std::invalid_argument";
    } catch (const std::invalid_argument& e) {
        std::string msg = e.what();
        EXPECT_FALSE(msg.empty());
    }
}

}  // namespace test
}  // namespace loramesher