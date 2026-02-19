// test/types/test_configurations/pin_configuration_test.cpp
#include <gtest/gtest.h>

#include "../src/types/configurations/pin_configuration.hpp"

namespace loramesher {
namespace test {

class PinConfigTest : public ::testing::Test {
   protected:
    void SetUp() override { defaultConfig = PinConfig::CreateDefault(); }

    PinConfig defaultConfig;
};

TEST_F(PinConfigTest, DefaultConstructorCreatesValidConfig) {
    EXPECT_TRUE(defaultConfig.IsValid());
    EXPECT_EQ(defaultConfig.getNss(), 18);
    EXPECT_EQ(defaultConfig.getReset(), 23);
    EXPECT_EQ(defaultConfig.getDio0(), 26);
    EXPECT_EQ(defaultConfig.getDio1(), 33);
}

TEST_F(PinConfigTest, CustomConstructorSetsValues) {
    PinConfig config(1, 2, 3, 4);
    EXPECT_EQ(config.getNss(), 1);
    EXPECT_EQ(config.getReset(), 2);
    EXPECT_EQ(config.getDio0(), 3);
    EXPECT_EQ(config.getDio1(), 4);
}

TEST_F(PinConfigTest, SettersValidateInput) {
    EXPECT_THROW(defaultConfig.setNss(-1), std::invalid_argument);
    EXPECT_THROW(defaultConfig.setReset(-1), std::invalid_argument);
    EXPECT_THROW(defaultConfig.setDio0(-1), std::invalid_argument);
    EXPECT_THROW(defaultConfig.setDio1(-1), std::invalid_argument);
}

TEST_F(PinConfigTest, ValidationWorksCorrectly) {
    PinConfig config(-1, -1, -1, -1);
    EXPECT_FALSE(config.IsValid());
    std::string errors = config.Validate();
    EXPECT_TRUE(errors.find("Invalid NSS pin") != std::string::npos);
    EXPECT_TRUE(errors.find("Invalid Reset pin") != std::string::npos);
    EXPECT_TRUE(errors.find("Invalid DIO0 pin") != std::string::npos);
    EXPECT_TRUE(errors.find("Invalid DIO1 pin") != std::string::npos);
}

TEST_F(PinConfigTest, DefaultConstructorHasNoCustomSpiPins) {
    EXPECT_EQ(defaultConfig.getSck(), -1);
    EXPECT_EQ(defaultConfig.getMiso(), -1);
    EXPECT_EQ(defaultConfig.getMosi(), -1);
    EXPECT_FALSE(defaultConfig.HasCustomSpiPins());
}

TEST_F(PinConfigTest, CustomSpiPinsSetViaConstructor) {
    PinConfig config(18, 23, 26, 33, 14, 12, 13);
    EXPECT_EQ(config.getSck(), 14);
    EXPECT_EQ(config.getMiso(), 12);
    EXPECT_EQ(config.getMosi(), 13);
    EXPECT_TRUE(config.HasCustomSpiPins());
}

TEST_F(PinConfigTest, CustomSpiPinsSetViaSetters) {
    defaultConfig.setSck(14);
    defaultConfig.setMiso(12);
    defaultConfig.setMosi(13);
    EXPECT_EQ(defaultConfig.getSck(), 14);
    EXPECT_EQ(defaultConfig.getMiso(), 12);
    EXPECT_EQ(defaultConfig.getMosi(), 13);
    EXPECT_TRUE(defaultConfig.HasCustomSpiPins());
}

TEST_F(PinConfigTest, DefaultConfigRemainsValidWithSpiPins) {
    PinConfig config(18, 23, 26, 33, 14, 12, 13);
    EXPECT_TRUE(config.IsValid());
}

}  // namespace test
}  // namespace loramesher
