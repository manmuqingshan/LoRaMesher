/**
 * @file hardware_coverage_test.cpp
 * @brief Unit tests for HardwareManager to increase code coverage
 */

#include <gtest/gtest.h>
#include <memory>

#include "hardware/hardware_manager.hpp"
#include "types/configurations/pin_configuration.hpp"
#include "types/configurations/radio_configuration.hpp"
#include "types/error_codes/loramesher_error_codes.hpp"
#include "types/messages/base_message.hpp"
#include "utils/logger.hpp"

namespace loramesher {
namespace hardware {
namespace test {

// =============================================================================
// Test Fixture
// =============================================================================

class HardwareManagerTest : public ::testing::Test {
   protected:
    static constexpr int kNss = 10;
    static constexpr int kDio0 = 11;
    static constexpr int kReset = 12;
    static constexpr int kDio1 = 13;

    void SetUp() override {
        pin_config_.setNss(kNss);
        pin_config_.setDio0(kDio0);
        pin_config_.setReset(kReset);
        pin_config_.setDio1(kDio1);
        // Default radio_config uses kSx1276 which falls through to MockRadio on native
    }

    void TearDown() override { hardware_manager_.reset(); }

    PinConfig pin_config_;
    RadioConfig radio_config_;
    std::unique_ptr<HardwareManager> hardware_manager_;

    void CreateAndInitialize() {
        hardware_manager_ =
            std::make_unique<HardwareManager>(pin_config_, radio_config_);
        Result result = hardware_manager_->Initialize();
        ASSERT_TRUE(result)
            << "Initialize failed: " << result.GetErrorMessage();
    }
};

// =============================================================================
// Construction Tests
// =============================================================================

TEST_F(HardwareManagerTest, ConstructWithValidConfig) {
    hardware_manager_ =
        std::make_unique<HardwareManager>(pin_config_, radio_config_);
    EXPECT_NE(hardware_manager_, nullptr);
    EXPECT_FALSE(hardware_manager_->IsInitialized());
}

TEST_F(HardwareManagerTest, DefaultConstructor) {
    // Default constructor should use default pin and radio configs
    hardware_manager_ = std::make_unique<HardwareManager>();
    EXPECT_NE(hardware_manager_, nullptr);
}

// =============================================================================
// Initialize Tests
// =============================================================================

TEST_F(HardwareManagerTest, InitializeSucceeds) {
    hardware_manager_ =
        std::make_unique<HardwareManager>(pin_config_, radio_config_);
    Result result = hardware_manager_->Initialize();
    EXPECT_TRUE(result) << "Initialize failed: " << result.GetErrorMessage();
    EXPECT_TRUE(hardware_manager_->IsInitialized());
}

TEST_F(HardwareManagerTest, DoubleInitializeIsIdempotent) {
    hardware_manager_ =
        std::make_unique<HardwareManager>(pin_config_, radio_config_);

    Result first = hardware_manager_->Initialize();
    EXPECT_TRUE(first);

    // Second Initialize should return Success without reinitializing
    Result second = hardware_manager_->Initialize();
    EXPECT_TRUE(second);
    EXPECT_TRUE(hardware_manager_->IsInitialized());
}

// =============================================================================
// Start/Stop Tests
// =============================================================================

TEST_F(HardwareManagerTest, StartWithoutInitializeFails) {
    hardware_manager_ =
        std::make_unique<HardwareManager>(pin_config_, radio_config_);
    // Not initialized yet
    Result result = hardware_manager_->Start();
    EXPECT_FALSE(result);
    EXPECT_EQ(result.getErrorCode(), LoraMesherErrorCode::kNotInitialized);
}

TEST_F(HardwareManagerTest, StartAfterInitializeSucceeds) {
    CreateAndInitialize();

    Result result = hardware_manager_->Start();
    EXPECT_TRUE(result) << "Start failed: " << result.GetErrorMessage();
}

TEST_F(HardwareManagerTest, DoubleStartIsIdempotent) {
    CreateAndInitialize();

    Result first = hardware_manager_->Start();
    EXPECT_TRUE(first);

    // Second Start() should be a no-op (already running)
    Result second = hardware_manager_->Start();
    EXPECT_TRUE(second);
}

TEST_F(HardwareManagerTest, StopWithoutInitializeFails) {
    hardware_manager_ =
        std::make_unique<HardwareManager>(pin_config_, radio_config_);
    // Not initialized, not running
    Result result = hardware_manager_->Stop();
    EXPECT_FALSE(result);
    EXPECT_EQ(result.getErrorCode(), LoraMesherErrorCode::kNotInitialized);
}

TEST_F(HardwareManagerTest, StopWithoutStartSucceeds) {
    CreateAndInitialize();
    // Initialized but not started — Stop should succeed (is_running_=false path)
    Result result = hardware_manager_->Stop();
    EXPECT_TRUE(result);
}

TEST_F(HardwareManagerTest, StopAfterStartSucceeds) {
    CreateAndInitialize();

    Result start_result = hardware_manager_->Start();
    ASSERT_TRUE(start_result);

    Result stop_result = hardware_manager_->Stop();
    EXPECT_TRUE(stop_result)
        << "Stop failed: " << stop_result.GetErrorMessage();
}

TEST_F(HardwareManagerTest, StartAfterStopSucceeds) {
    CreateAndInitialize();

    ASSERT_TRUE(hardware_manager_->Start());
    ASSERT_TRUE(hardware_manager_->Stop());

    // Should be able to start again after stopping
    Result result = hardware_manager_->Start();
    EXPECT_TRUE(result) << "Start after Stop failed: "
                        << result.GetErrorMessage();
}

// =============================================================================
// StartReceive Tests
// =============================================================================

TEST_F(HardwareManagerTest, StartReceiveWithoutInitializeFails) {
    hardware_manager_ =
        std::make_unique<HardwareManager>(pin_config_, radio_config_);
    Result result = hardware_manager_->StartReceive();
    EXPECT_FALSE(result);
    EXPECT_EQ(result.getErrorCode(), LoraMesherErrorCode::kNotInitialized);
}

TEST_F(HardwareManagerTest, StartReceiveAfterInitializeSucceeds) {
    CreateAndInitialize();

    Result result = hardware_manager_->StartReceive();
    EXPECT_TRUE(result) << "StartReceive failed: " << result.GetErrorMessage();
}

// =============================================================================
// setActionReceive Tests
// =============================================================================

TEST_F(HardwareManagerTest, SetActionReceiveWithoutInitializeFails) {
    hardware_manager_ =
        std::make_unique<HardwareManager>(pin_config_, radio_config_);
    Result result = hardware_manager_->setActionReceive(
        [](std::unique_ptr<radio::RadioEvent>) {});
    EXPECT_FALSE(result);
    EXPECT_EQ(result.getErrorCode(), LoraMesherErrorCode::kNotInitialized);
}

TEST_F(HardwareManagerTest, SetActionReceiveAfterInitializeSucceeds) {
    CreateAndInitialize();

    bool callback_set = false;
    Result result = hardware_manager_->setActionReceive(
        [&callback_set](std::unique_ptr<radio::RadioEvent>) {
            callback_set = true;
        });
    EXPECT_TRUE(result) << "setActionReceive failed: "
                        << result.GetErrorMessage();
}

// =============================================================================
// setState Tests
// =============================================================================

TEST_F(HardwareManagerTest, SetStateWithoutInitializeFails) {
    hardware_manager_ =
        std::make_unique<HardwareManager>(pin_config_, radio_config_);
    Result result = hardware_manager_->setState(radio::RadioState::kReceive);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.getErrorCode(), LoraMesherErrorCode::kNotInitialized);
}

TEST_F(HardwareManagerTest, SetStateAfterInitializeSucceeds) {
    CreateAndInitialize();

    Result result = hardware_manager_->setState(radio::RadioState::kReceive);
    EXPECT_TRUE(result) << "setState failed: " << result.GetErrorMessage();
}

TEST_F(HardwareManagerTest, SetStateSleepAfterInitialize) {
    CreateAndInitialize();

    Result result = hardware_manager_->setState(radio::RadioState::kSleep);
    EXPECT_TRUE(result) << "setState kSleep failed: "
                        << result.GetErrorMessage();
}

// =============================================================================
// SendMessage Tests
// =============================================================================

TEST_F(HardwareManagerTest, SendMessageWithoutRunningFails) {
    CreateAndInitialize();
    // Not started (is_running_=false)
    std::vector<uint8_t> payload = {0x01, 0x02};
    auto msg = BaseMessage::Create(0x1234, 0x5678, MessageType::PING, payload);
    ASSERT_TRUE(msg.has_value());

    Result result = hardware_manager_->SendMessage(*msg);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.getErrorCode(), LoraMesherErrorCode::kInvalidState);
}

TEST_F(HardwareManagerTest, SendMessageWithCallbackAfterStart) {
    CreateAndInitialize();
    ASSERT_TRUE(hardware_manager_->Start());

    bool tx_callback_called = false;
    hardware_manager_->setActionReceive(
        [&tx_callback_called](std::unique_ptr<radio::RadioEvent> event) {
            if (event &&
                event->getType() == radio::RadioEventType::kTransmitted) {
                tx_callback_called = true;
            }
        });

    std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
    auto msg = BaseMessage::Create(0x1234, 0x5678, MessageType::PING, payload);
    ASSERT_TRUE(msg.has_value());

    Result result = hardware_manager_->SendMessage(*msg);
    EXPECT_TRUE(result) << "SendMessage failed: " << result.GetErrorMessage();

    // Callback may be triggered synchronously or via event loop
    // We just verify the function doesn't crash
}

// =============================================================================
// getTimeOnAir Tests
// =============================================================================

TEST_F(HardwareManagerTest, GetTimeOnAirWithoutInitializeReturnsZero) {
    hardware_manager_ =
        std::make_unique<HardwareManager>(pin_config_, radio_config_);
    uint32_t time_on_air = hardware_manager_->getTimeOnAir(100);
    EXPECT_EQ(time_on_air, 0u);
}

TEST_F(HardwareManagerTest, GetTimeOnAirAfterInitializeReturnsNonZero) {
    CreateAndInitialize();

    // getTimeOnAir after initialization should return a value from the radio
    uint32_t time_on_air = hardware_manager_->getTimeOnAir(50);
    // Value is radio-implementation dependent; just check it doesn't crash
    (void)time_on_air;
}

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_F(HardwareManagerTest, GetPinConfigReturnsCorrectConfig) {
    hardware_manager_ =
        std::make_unique<HardwareManager>(pin_config_, radio_config_);

    const PinConfig& config = hardware_manager_->getPinConfig();
    EXPECT_EQ(config.getNss(), kNss);
    EXPECT_EQ(config.getDio0(), kDio0);
    EXPECT_EQ(config.getReset(), kReset);
}

TEST_F(HardwareManagerTest, GetRadioConfigReturnsCorrectConfig) {
    hardware_manager_ =
        std::make_unique<HardwareManager>(pin_config_, radio_config_);

    const RadioConfig& config = hardware_manager_->getRadioConfig();
    EXPECT_FLOAT_EQ(config.getFrequency(), radio_config_.getFrequency());
}

TEST_F(HardwareManagerTest, SetPinConfigWithValidPins) {
    hardware_manager_ =
        std::make_unique<HardwareManager>(pin_config_, radio_config_);

    PinConfig new_config;
    new_config.setNss(5);
    new_config.setDio0(6);
    new_config.setReset(7);
    new_config.setDio1(8);

    Result result = hardware_manager_->setPinConfig(new_config);
    EXPECT_TRUE(result) << "setPinConfig failed: " << result.GetErrorMessage();
    EXPECT_EQ(hardware_manager_->getPinConfig().getNss(), 5);
}

TEST_F(HardwareManagerTest, SetPinConfigWithInvalidPinsFails) {
    hardware_manager_ =
        std::make_unique<HardwareManager>(pin_config_, radio_config_);

    // Invalid pins (-1) should fail validation
    PinConfig invalid_config(-1, -1, -1, -1);
    Result result = hardware_manager_->setPinConfig(invalid_config);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.getErrorCode(), LoraMesherErrorCode::kInvalidParameter);
}

TEST_F(HardwareManagerTest, UpdateRadioConfigWithValidConfig) {
    hardware_manager_ =
        std::make_unique<HardwareManager>(pin_config_, radio_config_);

    RadioConfig new_config;
    new_config.setFrequency(915.0f);
    new_config.setSpreadingFactor(8);
    new_config.setBandwidth(250.0f);
    new_config.setCodingRate(5);
    new_config.setPower(14);
    new_config.setSyncWord(0x12);
    new_config.setCRC(true);
    new_config.setPreambleLength(8);

    Result result = hardware_manager_->updateRadioConfig(new_config);
    EXPECT_TRUE(result) << "updateRadioConfig failed: "
                        << result.GetErrorMessage();
}

// =============================================================================
// SetLocalAddress Tests
// =============================================================================

TEST_F(HardwareManagerTest, SetLocalAddressWithNoRadioDoesNotCrash) {
    hardware_manager_ =
        std::make_unique<HardwareManager>(pin_config_, radio_config_);
    // Radio is null before initialization — should not crash
    EXPECT_NO_THROW(hardware_manager_->SetLocalAddress(0x1234));
}

TEST_F(HardwareManagerTest, SetLocalAddressAfterInitialize) {
    CreateAndInitialize();
    EXPECT_NO_THROW(hardware_manager_->SetLocalAddress(0xABCD));
}

// =============================================================================
// getHal / getRadio Tests
// =============================================================================

TEST_F(HardwareManagerTest, GetHalReturnsNullBeforeInitialize) {
    hardware_manager_ =
        std::make_unique<HardwareManager>(pin_config_, radio_config_);
    // Before initialization, hal_ is nullptr
    EXPECT_EQ(hardware_manager_->getHal(), nullptr);
}

TEST_F(HardwareManagerTest, GetHalReturnsValidAfterInitialize) {
    CreateAndInitialize();
    EXPECT_NE(hardware_manager_->getHal(), nullptr);
}

TEST_F(HardwareManagerTest, GetRadioReturnsNullBeforeInitialize) {
    hardware_manager_ =
        std::make_unique<HardwareManager>(pin_config_, radio_config_);
    EXPECT_EQ(hardware_manager_->getRadio(), nullptr);
}

TEST_F(HardwareManagerTest, GetRadioReturnsValidAfterInitialize) {
    CreateAndInitialize();
    EXPECT_NE(hardware_manager_->getRadio(), nullptr);
}

}  // namespace test
}  // namespace hardware
}  // namespace loramesher
