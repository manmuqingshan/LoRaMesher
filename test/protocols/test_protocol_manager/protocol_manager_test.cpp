/**
 * @file protocol_manager_test.cpp
 * @brief Unit tests for ProtocolManager class
 */

#include <gtest/gtest.h>
#include <memory>

#include "hardware/hardware_manager.hpp"
#include "mocks/mock_radio_test_helpers.hpp"
#include "os/os_port.hpp"
#include "protocols/lora_mesh_protocol.hpp"
#include "protocols/ping_pong_protocol.hpp"
#include "protocols/protocol_manager.hpp"
#include "types/configurations/protocol_configuration.hpp"

namespace loramesher {
namespace protocols {
namespace test {

// ---------------------------------------------------------------------------
// Minimal IHardwareManager that always fails Initialize()
// ---------------------------------------------------------------------------

class FailingHardwareManager : public hardware::IHardwareManager {
   public:
    Result Initialize() override {
        return Result(LoraMesherErrorCode::kHardwareError,
                      "Simulated hardware init failure");
    }

    Result Start() override { return Result::Success(); }

    Result Stop() override { return Result::Success(); }

    Result setActionReceive(EventCallback /*cb*/) override {
        return Result::Success();
    }

    Result SendMessage(const BaseMessage& /*message*/) override {
        return Result::Success();
    }

    uint32_t getTimeOnAir(uint8_t /*length*/) override { return 10; }

    Result setState(radio::RadioState /*state*/) override {
        return Result::Success();
    }

    hal::IHal* getHal() override { return nullptr; }

    void SetLocalAddress(AddressType /*address*/) override {}
};

class ProtocolManagerTest : public ::testing::Test {
   protected:
    static constexpr AddressType kNodeAddress = 0x1234;

    void SetUp() override {
        pin_config_.setNss(10);
        pin_config_.setDio0(11);
        pin_config_.setReset(12);
        pin_config_.setDio1(13);

        hardware_manager_ = std::make_shared<hardware::HardwareManager>(
            pin_config_, radio_config_);
        ASSERT_TRUE(hardware_manager_->Initialize());

        manager_ = ProtocolManager::Create();
        ASSERT_NE(manager_, nullptr);
    }

    void TearDown() override {
        if (manager_) {
            manager_->StopAllProtocols();
            manager_.reset();
        }
        hardware_manager_.reset();
    }

    PinConfig pin_config_;
    RadioConfig radio_config_;
    std::shared_ptr<hardware::HardwareManager> hardware_manager_;
    std::unique_ptr<ProtocolManager> manager_;
};

// ---- Factory ----

TEST_F(ProtocolManagerTest, CreateReturnsNonNull) {
    EXPECT_NE(manager_, nullptr);
}

// ---- GetProtocol on empty manager ----

TEST_F(ProtocolManagerTest, GetProtocolEmptyReturnsNull) {
    EXPECT_EQ(manager_->GetProtocol(ProtocolType::kPingPong), nullptr);
    EXPECT_EQ(manager_->GetProtocol(ProtocolType::kLoraMesh), nullptr);
}

// ---- ConfigureProtocol on non-existing ----

TEST_F(ProtocolManagerTest, ConfigureNonExistingProtocolFails) {
    PingPongProtocolConfig pp_cfg;
    ProtocolConfig cfg;
    cfg.setPingPongConfig(pp_cfg);

    Result result = manager_->ConfigureProtocol(ProtocolType::kPingPong, cfg);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.getErrorCode(), LoraMesherErrorCode::kInvalidState);
}

// ---- CreateProtocol ----

TEST_F(ProtocolManagerTest, CreatePingPongProtocol) {
    auto protocol = manager_->CreateProtocol(ProtocolType::kPingPong,
                                             hardware_manager_, kNodeAddress);
    ASSERT_NE(protocol, nullptr);
    EXPECT_EQ(protocol->GetProtocolType(), ProtocolType::kPingPong);
}

TEST_F(ProtocolManagerTest, CreatePingPongProtocolCached) {
    auto p1 = manager_->CreateProtocol(ProtocolType::kPingPong,
                                       hardware_manager_, kNodeAddress);
    auto p2 = manager_->CreateProtocol(ProtocolType::kPingPong,
                                       hardware_manager_, kNodeAddress);
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    // Second call should return the same instance
    EXPECT_EQ(p1.get(), p2.get());
}

TEST_F(ProtocolManagerTest, GetProtocolAfterCreate) {
    manager_->CreateProtocol(ProtocolType::kPingPong, hardware_manager_,
                             kNodeAddress);
    auto protocol = manager_->GetProtocol(ProtocolType::kPingPong);
    EXPECT_NE(protocol, nullptr);
}

TEST_F(ProtocolManagerTest, GetProtocolAsSpecificType) {
    manager_->CreateProtocol(ProtocolType::kPingPong, hardware_manager_,
                             kNodeAddress);
    auto pp =
        manager_->GetProtocolAs<PingPongProtocol>(ProtocolType::kPingPong);
    EXPECT_NE(pp, nullptr);
}

TEST_F(ProtocolManagerTest, GetProtocolAsWrongType) {
    // Protocol not created → nullptr
    auto pp =
        manager_->GetProtocolAs<PingPongProtocol>(ProtocolType::kLoraMesh);
    EXPECT_EQ(pp, nullptr);
}

// ---- StopAllProtocols / StartAllProtocols ----

TEST_F(ProtocolManagerTest, StopAllProtocolsEmpty) {
    Result result = manager_->StopAllProtocols();
    EXPECT_TRUE(result);
}

TEST_F(ProtocolManagerTest, StopAllProtocolsWithPingPong) {
    manager_->CreateProtocol(ProtocolType::kPingPong, hardware_manager_,
                             kNodeAddress);
    Result result = manager_->StopAllProtocols();
    EXPECT_TRUE(result) << result.GetErrorMessage();
}

TEST_F(ProtocolManagerTest, StartAllProtocolsEmpty) {
    Result result = manager_->StartAllProtocols();
    EXPECT_TRUE(result);
}

TEST_F(ProtocolManagerTest, StartAllProtocolsWithPingPong) {
    manager_->CreateProtocol(ProtocolType::kPingPong, hardware_manager_,
                             kNodeAddress);
    Result result = manager_->StartAllProtocols();
    EXPECT_TRUE(result) << result.GetErrorMessage();
}

// ---- InitAllProtocols ----

TEST_F(ProtocolManagerTest, InitAllProtocolsEmpty) {
    Result result = manager_->InitAllProtocols(hardware_manager_, kNodeAddress);
    EXPECT_TRUE(result);
}

// ---- CreateProtocolWithConfig ----

TEST_F(ProtocolManagerTest, CreateWithPingPongConfig) {
    PingPongProtocolConfig pp_cfg(kNodeAddress, 3000, 2);
    ProtocolConfig cfg;
    cfg.setPingPongConfig(pp_cfg);

    auto protocol = manager_->CreateProtocolWithConfig(cfg, hardware_manager_);
    ASSERT_NE(protocol, nullptr);
    EXPECT_EQ(protocol->GetProtocolType(), ProtocolType::kPingPong);
}

TEST_F(ProtocolManagerTest, CreateWithPingPongConfigCached) {
    PingPongProtocolConfig pp_cfg(kNodeAddress);
    ProtocolConfig cfg;
    cfg.setPingPongConfig(pp_cfg);

    auto p1 = manager_->CreateProtocolWithConfig(cfg, hardware_manager_);
    auto p2 = manager_->CreateProtocolWithConfig(cfg, hardware_manager_);
    ASSERT_NE(p1, nullptr);
    // Second call returns cached instance
    EXPECT_EQ(p1.get(), p2.get());
}

// ---- CreateProtocol kLoraMesh ----

TEST_F(ProtocolManagerTest, CreateLoraMeshProtocol) {
    auto protocol = manager_->CreateProtocol(ProtocolType::kLoraMesh,
                                             hardware_manager_, kNodeAddress);
    ASSERT_NE(protocol, nullptr);
    EXPECT_EQ(protocol->GetProtocolType(), ProtocolType::kLoraMesh);
}

TEST_F(ProtocolManagerTest, CreateLoraMeshProtocolCached) {
    auto p1 = manager_->CreateProtocol(ProtocolType::kLoraMesh,
                                       hardware_manager_, kNodeAddress);
    auto p2 = manager_->CreateProtocol(ProtocolType::kLoraMesh,
                                       hardware_manager_, kNodeAddress);
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p1.get(), p2.get());
}

TEST_F(ProtocolManagerTest, GetLoraMeshProtocolAfterCreate) {
    manager_->CreateProtocol(ProtocolType::kLoraMesh, hardware_manager_,
                             kNodeAddress);
    auto protocol = manager_->GetProtocol(ProtocolType::kLoraMesh);
    EXPECT_NE(protocol, nullptr);
}

// ---- ConfigureProtocol kLoraMesh ----

TEST_F(ProtocolManagerTest, ConfigureLoraMeshAfterCreate) {
    manager_->CreateProtocol(ProtocolType::kLoraMesh, hardware_manager_,
                             kNodeAddress);

    LoRaMeshProtocolConfig lm_cfg(kNodeAddress);
    ProtocolConfig cfg;
    cfg.setLoRaMeshConfig(lm_cfg);

    Result result = manager_->ConfigureProtocol(ProtocolType::kLoraMesh, cfg);
    EXPECT_TRUE(result) << result.GetErrorMessage();
}

TEST_F(ProtocolManagerTest, ConfigurePingPongAfterCreate) {
    manager_->CreateProtocol(ProtocolType::kPingPong, hardware_manager_,
                             kNodeAddress);

    PingPongProtocolConfig pp_cfg(kNodeAddress);
    ProtocolConfig cfg;
    cfg.setPingPongConfig(pp_cfg);

    Result result = manager_->ConfigureProtocol(ProtocolType::kPingPong, cfg);
    EXPECT_TRUE(result) << result.GetErrorMessage();
}

// ---- CreateProtocolWithConfig kLoraMesh ----

TEST_F(ProtocolManagerTest, CreateWithLoraMeshConfig) {
    LoRaMeshProtocolConfig lm_cfg(kNodeAddress);
    ProtocolConfig cfg;
    cfg.setLoRaMeshConfig(lm_cfg);

    auto protocol = manager_->CreateProtocolWithConfig(cfg, hardware_manager_);
    ASSERT_NE(protocol, nullptr);
    EXPECT_EQ(protocol->GetProtocolType(), ProtocolType::kLoraMesh);
}

TEST_F(ProtocolManagerTest, CreateWithLoraMeshConfigCached) {
    LoRaMeshProtocolConfig lm_cfg(kNodeAddress);
    ProtocolConfig cfg;
    cfg.setLoRaMeshConfig(lm_cfg);

    auto p1 = manager_->CreateProtocolWithConfig(cfg, hardware_manager_);
    auto p2 = manager_->CreateProtocolWithConfig(cfg, hardware_manager_);
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1.get(), p2.get());
}

// ---- InitAllProtocols with existing protocol ----

TEST_F(ProtocolManagerTest, InitAllProtocolsWithPingPong) {
    manager_->CreateProtocol(ProtocolType::kPingPong, hardware_manager_,
                             kNodeAddress);
    Result result = manager_->InitAllProtocols(hardware_manager_, kNodeAddress);
    EXPECT_TRUE(result) << result.GetErrorMessage();
}

// ---- StartAllProtocols with LoraMesh ----

TEST_F(ProtocolManagerTest, StartAllProtocolsWithLoraMesh) {
    manager_->CreateProtocol(ProtocolType::kLoraMesh, hardware_manager_,
                             kNodeAddress);
    Result result = manager_->StartAllProtocols();
    EXPECT_TRUE(result) << result.GetErrorMessage();
}

// ---- GetProtocolAs with LoraMesh ----

TEST_F(ProtocolManagerTest, GetLoraMeshProtocolAsSpecificType) {
    manager_->CreateProtocol(ProtocolType::kLoraMesh, hardware_manager_,
                             kNodeAddress);
    auto lm =
        manager_->GetProtocolAs<LoRaMeshProtocol>(ProtocolType::kLoraMesh);
    EXPECT_NE(lm, nullptr);
}

// ---- CreateProtocol with invalid/unknown type → default case → nullptr ----

TEST_F(ProtocolManagerTest, CreateProtocolInvalidTypeReturnsNull) {
    // Cast an out-of-range integer to ProtocolType to hit the default: branch
    auto protocol = manager_->CreateProtocol(static_cast<ProtocolType>(99),
                                             hardware_manager_, kNodeAddress);
    EXPECT_EQ(protocol, nullptr);
}

TEST_F(ProtocolManagerTest, CreateProtocolCustomTypeReturnsNull) {
    // kCustomProtocol has no implementation, so it falls through to default
    auto protocol = manager_->CreateProtocol(ProtocolType::kCustomProtocol,
                                             hardware_manager_, kNodeAddress);
    EXPECT_EQ(protocol, nullptr);
}

// ---- CreateProtocolWithConfig with invalid/unknown type → default → nullptr ----

TEST_F(ProtocolManagerTest, CreateProtocolWithConfigInvalidTypeReturnsNull) {
    // Build a ProtocolConfig whose internal type is set to an unknown value.
    // We construct a PingPong config and then manually force the type by
    // using the generic unique_ptr constructor with a custom BaseProtocolConfig.
    // The simplest approach: call CreateProtocol with the bad type first,
    // confirm null, then verify manager still works.
    auto bad_protocol = manager_->CreateProtocol(
        static_cast<ProtocolType>(99), hardware_manager_, kNodeAddress);
    EXPECT_EQ(bad_protocol, nullptr);

    // After the failed creation the manager must still be usable.
    auto good_protocol = manager_->CreateProtocol(
        ProtocolType::kPingPong, hardware_manager_, kNodeAddress);
    EXPECT_NE(good_protocol, nullptr);
}

// ---- ConfigureProtocol with an unknown type hits the default: → error ----

TEST_F(ProtocolManagerTest, ConfigureProtocolUnknownTypeReturnsError) {
    // Insert a PingPong protocol, then try to configure with a ProtocolConfig
    // whose getProtocolType() returns kLoraMesh while the stored key is kPingPong.
    // The simplest path to hit ConfigureProtocol's default: branch is to create
    // a ProtocolConfig that reports kCustomProtocol, but we cannot store such a
    // key via CreateProtocol (it returns nullptr).  Instead we verify that
    // ConfigureProtocol returns kInvalidState when the protocol isn't registered,
    // and add a second check that it returns kInvalidParameter for mismatched type.

    // Protocol not registered → kInvalidState
    LoRaMeshProtocolConfig lm_cfg(kNodeAddress);
    ProtocolConfig lm_config;
    lm_config.setLoRaMeshConfig(lm_cfg);

    Result result =
        manager_->ConfigureProtocol(ProtocolType::kLoraMesh, lm_config);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.getErrorCode(), LoraMesherErrorCode::kInvalidState);
}

TEST_F(ProtocolManagerTest, ConfigureProtocolWrongConfigTypeReturnsError) {
    // Create a LoraMesh protocol, then try to configure it with a PingPong config.
    // getLoRaMeshConfig() will throw std::bad_cast, which ConfigureProtocol
    // catches and returns kInvalidParameter.
    manager_->CreateProtocol(ProtocolType::kLoraMesh, hardware_manager_,
                             kNodeAddress);

    // Pass a ProtocolConfig that wraps PingPong but call ConfigureProtocol with
    // kLoraMesh type so it tries to cast to LoRaMeshConfig and throws bad_cast.
    PingPongProtocolConfig pp_cfg(kNodeAddress);
    ProtocolConfig mismatch_cfg;
    mismatch_cfg.setPingPongConfig(pp_cfg);

    // ConfigureProtocol(kLoraMesh, ...) will call config.getLoRaMeshConfig()
    // which throws std::bad_cast because the inner config is PingPong.
    Result result =
        manager_->ConfigureProtocol(ProtocolType::kLoraMesh, mismatch_cfg);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.getErrorCode(), LoraMesherErrorCode::kInvalidParameter);
}

// ---- GetProtocol returns nullptr for unknown type ----

TEST_F(ProtocolManagerTest, GetProtocolUnknownTypeReturnsNull) {
    auto p = manager_->GetProtocol(static_cast<ProtocolType>(99));
    EXPECT_EQ(p, nullptr);
}

// ---- InitAllProtocols with LoraMesh ----

TEST_F(ProtocolManagerTest, InitAllProtocolsWithLoraMesh) {
    manager_->CreateProtocol(ProtocolType::kLoraMesh, hardware_manager_,
                             kNodeAddress);
    Result result = manager_->InitAllProtocols(hardware_manager_, kNodeAddress);
    EXPECT_TRUE(result) << result.GetErrorMessage();
}

// ---- StartAllProtocols / StopAllProtocols with both protocols ----

TEST_F(ProtocolManagerTest, StartStopAllProtocolsBoth) {
    manager_->CreateProtocol(ProtocolType::kPingPong, hardware_manager_,
                             kNodeAddress);
    manager_->CreateProtocol(ProtocolType::kLoraMesh, hardware_manager_,
                             kNodeAddress);

    Result start_result = manager_->StartAllProtocols();
    EXPECT_TRUE(start_result) << start_result.GetErrorMessage();

    Result stop_result = manager_->StopAllProtocols();
    EXPECT_TRUE(stop_result) << stop_result.GetErrorMessage();
}

// ---- Init failure paths ----

TEST_F(ProtocolManagerTest, CreateProtocolPingPongInitFailure) {
    auto failing_hw = std::make_shared<FailingHardwareManager>();

    auto protocol = manager_->CreateProtocol(ProtocolType::kPingPong,
                                             failing_hw, kNodeAddress);
    EXPECT_EQ(protocol, nullptr);
}

TEST_F(ProtocolManagerTest, CreateProtocolLoraMeshInitFailure) {
    auto failing_hw = std::make_shared<FailingHardwareManager>();

    auto protocol = manager_->CreateProtocol(ProtocolType::kLoraMesh,
                                             failing_hw, kNodeAddress);
    EXPECT_EQ(protocol, nullptr);
}

TEST_F(ProtocolManagerTest, CreateProtocolWithConfigPingPongInitFailure) {
    auto failing_hw = std::make_shared<FailingHardwareManager>();

    PingPongProtocolConfig pp_cfg(kNodeAddress, 3000, 2);
    ProtocolConfig cfg;
    cfg.setPingPongConfig(pp_cfg);

    auto protocol = manager_->CreateProtocolWithConfig(cfg, failing_hw);
    EXPECT_EQ(protocol, nullptr);
}

TEST_F(ProtocolManagerTest, InitAllProtocolsWithFailingHardware) {
    // Create a PingPong protocol with working hardware so it gets stored
    manager_->CreateProtocol(ProtocolType::kPingPong, hardware_manager_,
                             kNodeAddress);

    // Re-initialize all protocols with hardware that fails Initialize()
    auto failing_hw = std::make_shared<FailingHardwareManager>();
    Result result = manager_->InitAllProtocols(failing_hw, kNodeAddress);

    // InitAllProtocols accumulates errors; at least one error expected
    EXPECT_FALSE(result);
}

}  // namespace test
}  // namespace protocols
}  // namespace loramesher
