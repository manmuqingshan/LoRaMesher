/**
 * @file ping_pong_protocol_test.cpp
 * @brief Unit tests for PingPongProtocol class
 */

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <thread>

#include "hardware/hardware_manager.hpp"
#include "hardware/radiolib/radiolib_radio.hpp"
#include "mocks/mock_radio_test_helpers.hpp"
#include "os/os_port.hpp"
#include "protocols/ping_pong_protocol.hpp"
#include "types/messages/ping_pong/ping_pong_message.hpp"
#include "types/radio/radio_event.hpp"

namespace loramesher {
namespace protocols {
namespace test {

/**
 * @brief Test fixture for PingPongProtocol tests
 */
class PingPongProtocolTest : public ::testing::Test {
   protected:
    static constexpr AddressType kNodeAddress = 0x1234;
    static constexpr AddressType kRemoteAddress = 0x5678;

    void SetUp() override {
        // Configure hardware
        pin_config_.setNss(10);
        pin_config_.setDio0(11);
        pin_config_.setReset(12);
        pin_config_.setDio1(13);

        // Create hardware manager with mock radio
        hardware_manager_ = std::make_shared<hardware::HardwareManager>(
            pin_config_, radio_config_);
        ASSERT_TRUE(hardware_manager_->Initialize())
            << "Hardware initialization failed";
    }

    void TearDown() override {
        if (protocol_) {
            protocol_->Stop();
            protocol_.reset();
        }
        hardware_manager_.reset();
    }

    /**
     * @brief Initialize protocol and return result
     */
    Result InitProtocol() {
        protocol_ = std::make_unique<PingPongProtocol>();
        return protocol_->Init(hardware_manager_, kNodeAddress);
    }

    /**
     * @brief Create a serialized PingPong ping message for radio event injection
     */
    std::vector<uint8_t> MakePingBytes(AddressType src, uint16_t seq,
                                       uint32_t ts) {
        auto msg = PingPongMessage::Create(kNodeAddress, src,
                                           PingPongSubtype::PING, seq, ts);
        EXPECT_TRUE(msg.has_value());
        auto serialized = msg->Serialize();
        EXPECT_TRUE(serialized.has_value());
        return *serialized;
    }

    /**
     * @brief Create a serialized PingPong pong message for radio event injection
     */
    std::vector<uint8_t> MakePongBytes(AddressType src, uint16_t seq,
                                       uint32_t ts) {
        auto msg = PingPongMessage::Create(kNodeAddress, src,
                                           PingPongSubtype::PONG, seq, ts);
        EXPECT_TRUE(msg.has_value());
        auto serialized = msg->Serialize();
        EXPECT_TRUE(serialized.has_value());
        return *serialized;
    }

    PinConfig pin_config_;
    RadioConfig radio_config_;
    std::shared_ptr<hardware::HardwareManager> hardware_manager_;
    std::unique_ptr<PingPongProtocol> protocol_;
};

// ---- Lifecycle tests ----

TEST_F(PingPongProtocolTest, InitSuccess) {
    Result result = InitProtocol();
    EXPECT_TRUE(result) << result.GetErrorMessage();
}

TEST_F(PingPongProtocolTest, StartAfterInit) {
    ASSERT_TRUE(InitProtocol());
    Result result = protocol_->Start();
    EXPECT_TRUE(result) << result.GetErrorMessage();
}

TEST_F(PingPongProtocolTest, StopAfterInit) {
    ASSERT_TRUE(InitProtocol());
    Result result = protocol_->Stop();
    EXPECT_TRUE(result) << result.GetErrorMessage();
    // Double stop is idempotent
    Result result2 = protocol_->Stop();
    EXPECT_TRUE(result2) << result2.GetErrorMessage();
}

TEST_F(PingPongProtocolTest, StartWithoutHardwareFails) {
    protocol_ = std::make_unique<PingPongProtocol>();
    // No hardware set — Start() should fail
    Result result = protocol_->Start();
    EXPECT_FALSE(result);
}

// ---- SendPing tests ----

TEST_F(PingPongProtocolTest, SendPingSuccess) {
    ASSERT_TRUE(InitProtocol());
    ASSERT_TRUE(protocol_->Start())
        << "Hardware must be started before SendPing";
    Result result = protocol_->SendPing(kRemoteAddress, kNodeAddress, 1000);
    EXPECT_TRUE(result) << result.GetErrorMessage();
}

TEST_F(PingPongProtocolTest, SendPingWithCallback) {
    ASSERT_TRUE(InitProtocol());
    ASSERT_TRUE(protocol_->Start());

    bool callback_called = false;
    auto callback = [&](AddressType addr, uint32_t rtt, bool success) {
        callback_called = true;
        (void)addr;
        (void)rtt;
        (void)success;
    };

    Result result =
        protocol_->SendPing(kRemoteAddress, kNodeAddress, 1000, callback);
    EXPECT_TRUE(result) << result.GetErrorMessage();
    // Callback should not be called immediately (only on response or timeout)
    EXPECT_FALSE(callback_called);
}

TEST_F(PingPongProtocolTest, SendPingZeroSource) {
    ASSERT_TRUE(InitProtocol());
    ASSERT_TRUE(protocol_->Start());
    // source=0 should use node_address_
    Result result = protocol_->SendPing(kRemoteAddress, 0, 1000);
    EXPECT_TRUE(result) << result.GetErrorMessage();
}

// ---- ProcessReceivedRadioEvent tests ----

TEST_F(PingPongProtocolTest, ProcessPingEvent) {
    ASSERT_TRUE(InitProtocol());
    ASSERT_TRUE(
        protocol_->Start());  // hardware must be running to send PONG response

    auto ping_bytes = MakePingBytes(kRemoteAddress, 42, 12345);
    auto base_msg = BaseMessage::CreateFromSerialized(ping_bytes);
    ASSERT_TRUE(base_msg.has_value());

    auto event = std::make_unique<radio::RadioEvent>(
        radio::RadioEventType::kReceived,
        std::make_unique<BaseMessage>(*base_msg));

    Result result = protocol_->ProcessReceivedRadioEvent(std::move(event));
    EXPECT_TRUE(result) << result.GetErrorMessage();
}

TEST_F(PingPongProtocolTest, ProcessPongEventNoPendingPing) {
    ASSERT_TRUE(InitProtocol());

    auto pong_bytes = MakePongBytes(kRemoteAddress, 99, 10000);
    auto base_msg = BaseMessage::CreateFromSerialized(pong_bytes);
    ASSERT_TRUE(base_msg.has_value());

    auto event = std::make_unique<radio::RadioEvent>(
        radio::RadioEventType::kReceived,
        std::make_unique<BaseMessage>(*base_msg));

    // No pending ping for kRemoteAddress → should return error
    Result result = protocol_->ProcessReceivedRadioEvent(std::move(event));
    EXPECT_FALSE(result);
    EXPECT_EQ(result.getErrorCode(), LoraMesherErrorCode::kInvalidState);
}

TEST_F(PingPongProtocolTest, ProcessPongAfterPing) {
    ASSERT_TRUE(InitProtocol());
    ASSERT_TRUE(protocol_->Start());

    bool success_cb = false;
    uint32_t rtt_cb = 0;
    auto callback = [&](AddressType addr, uint32_t rtt, bool ok) {
        success_cb = ok;
        rtt_cb = rtt;
        (void)addr;
    };

    // Send ping first (seq=0)
    protocol_->SendPing(kRemoteAddress, kNodeAddress, 5000, callback);

    // Now create a PONG from remote with seq=0
    auto pong_bytes = MakePongBytes(kRemoteAddress, 0, 12345);
    auto base_msg = BaseMessage::CreateFromSerialized(pong_bytes);
    ASSERT_TRUE(base_msg.has_value());

    auto event = std::make_unique<radio::RadioEvent>(
        radio::RadioEventType::kReceived,
        std::make_unique<BaseMessage>(*base_msg));

    Result result = protocol_->ProcessReceivedRadioEvent(std::move(event));
    EXPECT_TRUE(result) << result.GetErrorMessage();
    EXPECT_TRUE(success_cb);
    (void)rtt_cb;
}

TEST_F(PingPongProtocolTest, ProcessNonReceivedEventFails) {
    ASSERT_TRUE(InitProtocol());

    auto event = std::make_unique<radio::RadioEvent>(
        radio::RadioEventType::kTransmitted);

    Result result = protocol_->ProcessReceivedRadioEvent(std::move(event));
    EXPECT_FALSE(result);
    EXPECT_EQ(result.getErrorCode(), LoraMesherErrorCode::kInvalidParameter);
}

TEST_F(PingPongProtocolTest, ProcessEventNoMessage) {
    ASSERT_TRUE(InitProtocol());

    // Event with null message
    auto event =
        std::make_unique<radio::RadioEvent>(radio::RadioEventType::kReceived);

    Result result = protocol_->ProcessReceivedRadioEvent(std::move(event));
    EXPECT_FALSE(result);
}

// ---- CheckTimeouts ----

TEST_F(PingPongProtocolTest, CheckTimeoutsNoTimeouts) {
    ASSERT_TRUE(InitProtocol());
    ASSERT_TRUE(protocol_->Start());

    // Send a ping with long timeout
    protocol_->SendPing(kRemoteAddress, kNodeAddress, 60000);

    // CheckTimeouts immediately — not expired yet
    protocol_->CheckTimeouts();
    // No crash
}

TEST_F(PingPongProtocolTest, CheckTimeoutsExpiredCallsCallback) {
    ASSERT_TRUE(InitProtocol());
    ASSERT_TRUE(protocol_->Start());

    bool timeout_called = false;
    bool success_val = true;
    auto callback = [&](AddressType addr, uint32_t rtt, bool ok) {
        timeout_called = true;
        success_val = ok;
        (void)addr;
        (void)rtt;
    };

    // Send ping with very short timeout (1ms)
    protocol_->SendPing(kRemoteAddress, kNodeAddress, 1, callback);

    // Wait more than 1ms
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // CheckTimeouts should trigger the callback
    protocol_->CheckTimeouts();

    EXPECT_TRUE(timeout_called);
    EXPECT_FALSE(success_val);  // timeout → success=false
}

TEST_F(PingPongProtocolTest, CheckTimeoutsMultiplePingsSomeExpired) {
    ASSERT_TRUE(InitProtocol());
    ASSERT_TRUE(protocol_->Start());

    int expired_count = 0;
    int not_expired_count = 0;

    auto callback_expired = [&](AddressType, uint32_t, bool ok) {
        if (!ok) {
            expired_count++;
        }
    };
    auto callback_not_expired = [&](AddressType, uint32_t, bool) {
        not_expired_count++;
    };

    // Short-timeout ping (1ms) — will expire
    protocol_->SendPing(kRemoteAddress, kNodeAddress, 1, callback_expired);

    // Wait for the short one to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // Long-timeout ping (60s) — will not expire
    protocol_->SendPing(0xAAAA, kNodeAddress, 60000, callback_not_expired);

    protocol_->CheckTimeouts();

    EXPECT_EQ(expired_count, 1);
    EXPECT_EQ(not_expired_count, 0);
}

TEST_F(PingPongProtocolTest, ProcessPongWithWrongSequenceNumber) {
    ASSERT_TRUE(InitProtocol());
    ASSERT_TRUE(protocol_->Start());

    // Send ping (seq=0 will be assigned)
    protocol_->SendPing(kRemoteAddress, kNodeAddress, 5000);

    // Receive pong with a different sequence number (99, not 0)
    auto pong_bytes = MakePongBytes(kRemoteAddress, 99, 12345);
    auto base_msg = BaseMessage::CreateFromSerialized(pong_bytes);
    ASSERT_TRUE(base_msg.has_value());

    auto event = std::make_unique<radio::RadioEvent>(
        radio::RadioEventType::kReceived,
        std::make_unique<BaseMessage>(*base_msg));

    // Wrong sequence number → error
    Result result = protocol_->ProcessReceivedRadioEvent(std::move(event));
    EXPECT_FALSE(result);
    EXPECT_EQ(result.getErrorCode(), LoraMesherErrorCode::kInvalidState);
}

// ---- SendMessage ----

TEST_F(PingPongProtocolTest, SendMessageWithoutHardwareFails) {
    protocol_ = std::make_unique<PingPongProtocol>();
    // Not initialized — no hardware
    auto ping_msg = PingPongMessage::Create(kRemoteAddress, kNodeAddress,
                                            PingPongSubtype::PING, 1, 0);
    ASSERT_TRUE(ping_msg.has_value());
    Result result = protocol_->SendMessage(ping_msg->ToBaseMessage());
    EXPECT_FALSE(result);
}

TEST_F(PingPongProtocolTest, SendMessageAfterInit) {
    ASSERT_TRUE(InitProtocol());
    ASSERT_TRUE(protocol_->Start());

    auto ping_msg = PingPongMessage::Create(kRemoteAddress, kNodeAddress,
                                            PingPongSubtype::PING, 1, 0);
    ASSERT_TRUE(ping_msg.has_value());
    Result result = protocol_->SendMessage(ping_msg->ToBaseMessage());
    EXPECT_TRUE(result) << result.GetErrorMessage();
}

// ---- Multi-destination CheckTimeouts coverage ----

TEST_F(PingPongProtocolTest, CheckTimeoutsAllDestsExpiredMapBecomesEmpty) {
    ASSERT_TRUE(InitProtocol());
    ASSERT_TRUE(protocol_->Start());

    static constexpr AddressType kDest1 = 0x0001;
    static constexpr AddressType kDest2 = 0x0002;
    static constexpr AddressType kDest3 = 0x0003;

    int fired = 0;
    auto cb = [&](AddressType, uint32_t, bool ok) {
        if (!ok) {
            fired++;
        }
    };

    // Send pings to 3 distinct destinations with 1ms timeout each
    protocol_->SendPing(kDest1, kNodeAddress, 1, cb);
    protocol_->SendPing(kDest2, kNodeAddress, 1, cb);
    protocol_->SendPing(kDest3, kNodeAddress, 1, cb);

    // Wait for all to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // First CheckTimeouts should fire all 3 callbacks and clear the map
    protocol_->CheckTimeouts();
    EXPECT_EQ(fired, 3);

    // Second CheckTimeouts call with no pings pending should be a no-op
    int fired_after = fired;
    protocol_->CheckTimeouts();
    EXPECT_EQ(fired, fired_after);
}

TEST_F(PingPongProtocolTest, CheckTimeoutsNoCallbackStillRemoves) {
    ASSERT_TRUE(InitProtocol());
    ASSERT_TRUE(protocol_->Start());

    // Send ping with nullptr callback — should not crash on timeout
    Result result =
        protocol_->SendPing(kRemoteAddress, kNodeAddress, 1, nullptr);
    EXPECT_TRUE(result) << result.GetErrorMessage();

    // Wait for timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // CheckTimeouts must handle nullptr callback without crashing
    EXPECT_NO_THROW(protocol_->CheckTimeouts());

    // Subsequent CheckTimeouts is a no-op — no crash
    EXPECT_NO_THROW(protocol_->CheckTimeouts());
}

TEST_F(PingPongProtocolTest, CheckTimeoutsMultipleSeqNumsSameDestSomeExpire) {
    ASSERT_TRUE(InitProtocol());
    ASSERT_TRUE(protocol_->Start());

    int expired_count = 0;
    int long_timeout_count = 0;

    auto cb_short = [&](AddressType, uint32_t, bool ok) {
        if (!ok) {
            expired_count++;
        }
    };
    auto cb_long = [&](AddressType, uint32_t, bool) {
        long_timeout_count++;
    };

    // Send 3 pings to the same destination
    // First two with very short timeouts (will expire)
    protocol_->SendPing(kRemoteAddress, kNodeAddress, 1, cb_short);
    protocol_->SendPing(kRemoteAddress, kNodeAddress, 1, cb_short);
    // Third with long timeout (will NOT expire)
    protocol_->SendPing(kRemoteAddress, kNodeAddress, 60000, cb_long);

    // Wait for first two to expire but not the third
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    protocol_->CheckTimeouts();

    // First two should have timed out
    EXPECT_EQ(expired_count, 2);
    // The long-timeout ping must still be pending (callback not called yet)
    EXPECT_EQ(long_timeout_count, 0);
}

}  // namespace test
}  // namespace protocols
}  // namespace loramesher
