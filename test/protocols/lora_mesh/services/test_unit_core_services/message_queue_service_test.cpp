/**
 * @file message_queue_service_test.cpp
 * @brief Unit tests for MessageQueueService class
 */

#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "protocols/lora_mesh/services/message_queue_service.hpp"
#include "types/messages/base_message.hpp"

namespace loramesher {
namespace protocols {
namespace lora_mesh {
namespace test {

using SlotType = types::protocols::lora_mesh::SlotAllocation::SlotType;

class MessageQueueServiceTest : public ::testing::Test {
   protected:
    static constexpr AddressType kSrc = 0x1234;
    static constexpr AddressType kDst = 0x5678;

    // max_queue_size = 5 for all tests
    MessageQueueServiceTest() : service_(5) {}

    std::unique_ptr<BaseMessage> MakeMessage(
        MessageType type = MessageType::PING) {
        auto opt = BaseMessage::Create(kDst, kSrc, type,
                                       std::vector<uint8_t>{0x01, 0x02});
        EXPECT_TRUE(opt.has_value());
        return std::make_unique<BaseMessage>(*opt);
    }

    MessageQueueService service_;
};

// ---- AddMessageToQueue ----

TEST_F(MessageQueueServiceTest, AddAndSizeIncreases) {
    service_.AddMessageToQueue(SlotType::TX, MakeMessage());
    EXPECT_EQ(service_.GetQueueSize(SlotType::TX), 1u);
    EXPECT_FALSE(service_.IsQueueEmpty(SlotType::TX));
}

TEST_F(MessageQueueServiceTest, AddInvalidSlotTypeZeroIgnored) {
    service_.AddMessageToQueue(static_cast<SlotType>(0), MakeMessage());
    EXPECT_EQ(service_.GetTotalMessageCount(), 0u);
}

TEST_F(MessageQueueServiceTest, AddInvalidSlotTypeOutOfRangeIgnored) {
    service_.AddMessageToQueue(static_cast<SlotType>(10), MakeMessage());
    EXPECT_EQ(service_.GetTotalMessageCount(), 0u);
}

TEST_F(MessageQueueServiceTest, AddToAllValidSlotTypes) {
    // SlotType values TX=1 through SYNC_BEACON_RX=9
    for (int i = 1; i <= 9; ++i) {
        service_.AddMessageToQueue(static_cast<SlotType>(i), MakeMessage());
    }
    EXPECT_EQ(service_.GetTotalMessageCount(), 9u);
}

TEST_F(MessageQueueServiceTest, QueueFullRejectsNewMessage) {
    // Fill to capacity (5) with PING messages
    for (int i = 0; i < 5; ++i) {
        Result r = service_.AddMessageToQueue(SlotType::TX,
                                              MakeMessage(MessageType::PING));
        EXPECT_TRUE(r.IsSuccess());
    }
    EXPECT_EQ(service_.GetQueueSize(SlotType::TX), 5u);

    // The next add must be rejected with kQueueFull and must not change size
    Result r =
        service_.AddMessageToQueue(SlotType::TX, MakeMessage(MessageType::DATA));
    EXPECT_FALSE(r.IsSuccess());
    EXPECT_EQ(r.getErrorCode(), LoraMesherErrorCode::kQueueFull);
    EXPECT_EQ(service_.GetQueueSize(SlotType::TX), 5u);

    // The five originals remain in FIFO order; the rejected DATA is gone
    for (int i = 0; i < 5; ++i) {
        auto extracted = service_.ExtractMessageOfType(SlotType::TX);
        ASSERT_NE(extracted, nullptr);
        EXPECT_EQ(extracted->GetType(), MessageType::PING);
    }
    EXPECT_TRUE(service_.IsQueueEmpty(SlotType::TX));
}

TEST_F(MessageQueueServiceTest, AddMessageReturnsSuccessWhenSpaceAvailable) {
    Result r = service_.AddMessageToQueue(SlotType::TX, MakeMessage());
    EXPECT_TRUE(r.IsSuccess());
    EXPECT_EQ(r.getErrorCode(), LoraMesherErrorCode::kSuccess);
}

TEST_F(MessageQueueServiceTest, AddMessageReturnsErrorForInvalidSlotType) {
    Result r0 =
        service_.AddMessageToQueue(static_cast<SlotType>(0), MakeMessage());
    EXPECT_FALSE(r0.IsSuccess());
    EXPECT_EQ(r0.getErrorCode(), LoraMesherErrorCode::kInvalidArgument);

    Result r10 =
        service_.AddMessageToQueue(static_cast<SlotType>(10), MakeMessage());
    EXPECT_FALSE(r10.IsSuccess());
    EXPECT_EQ(r10.getErrorCode(), LoraMesherErrorCode::kInvalidArgument);

    EXPECT_EQ(service_.GetTotalMessageCount(), 0u);
}

TEST_F(MessageQueueServiceTest, AddMultipleMessages) {
    service_.AddMessageToQueue(SlotType::RX, MakeMessage());
    service_.AddMessageToQueue(SlotType::RX, MakeMessage());
    service_.AddMessageToQueue(SlotType::RX, MakeMessage());
    EXPECT_EQ(service_.GetQueueSize(SlotType::RX), 3u);
}

// ---- ExtractMessageOfType ----

TEST_F(MessageQueueServiceTest, ExtractFromEmptyReturnsNull) {
    auto msg = service_.ExtractMessageOfType(SlotType::RX);
    EXPECT_EQ(msg, nullptr);
}

TEST_F(MessageQueueServiceTest, ExtractFromInvalidTypeReturnsNull) {
    EXPECT_EQ(service_.ExtractMessageOfType(static_cast<SlotType>(0)), nullptr);
    EXPECT_EQ(service_.ExtractMessageOfType(static_cast<SlotType>(10)),
              nullptr);
}

TEST_F(MessageQueueServiceTest, ExtractReturnsAndRemovesMessage) {
    service_.AddMessageToQueue(SlotType::TX, MakeMessage());
    auto msg = service_.ExtractMessageOfType(SlotType::TX);
    EXPECT_NE(msg, nullptr);
    EXPECT_TRUE(service_.IsQueueEmpty(SlotType::TX));
}

TEST_F(MessageQueueServiceTest, ExtractFIFOOrder) {
    service_.AddMessageToQueue(SlotType::RX, MakeMessage(MessageType::PING));
    service_.AddMessageToQueue(SlotType::RX, MakeMessage(MessageType::DATA));
    auto first = service_.ExtractMessageOfType(SlotType::RX);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->GetType(), MessageType::PING);
    auto second = service_.ExtractMessageOfType(SlotType::RX);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->GetType(), MessageType::DATA);
}

TEST_F(MessageQueueServiceTest, ExtractUntilEmpty) {
    service_.AddMessageToQueue(SlotType::SLEEP, MakeMessage());
    service_.AddMessageToQueue(SlotType::SLEEP, MakeMessage());
    EXPECT_NE(service_.ExtractMessageOfType(SlotType::SLEEP), nullptr);
    EXPECT_NE(service_.ExtractMessageOfType(SlotType::SLEEP), nullptr);
    EXPECT_EQ(service_.ExtractMessageOfType(SlotType::SLEEP), nullptr);
}

// ---- IsQueueEmpty ----

TEST_F(MessageQueueServiceTest, IsQueueEmptyInitially) {
    EXPECT_TRUE(service_.IsQueueEmpty(SlotType::TX));
    EXPECT_TRUE(service_.IsQueueEmpty(SlotType::DISCOVERY_RX));
    EXPECT_TRUE(service_.IsQueueEmpty(SlotType::SYNC_BEACON_TX));
}

TEST_F(MessageQueueServiceTest, IsQueueEmptyInvalidTypeReturnsTrue) {
    EXPECT_TRUE(service_.IsQueueEmpty(static_cast<SlotType>(0)));
    EXPECT_TRUE(service_.IsQueueEmpty(static_cast<SlotType>(11)));
}

TEST_F(MessageQueueServiceTest, IsQueueEmptyAfterClear) {
    service_.AddMessageToQueue(SlotType::CONTROL_TX, MakeMessage());
    service_.ClearQueue(SlotType::CONTROL_TX);
    EXPECT_TRUE(service_.IsQueueEmpty(SlotType::CONTROL_TX));
}

// ---- GetQueueSize ----

TEST_F(MessageQueueServiceTest, GetQueueSizeInvalidReturnsZero) {
    EXPECT_EQ(service_.GetQueueSize(static_cast<SlotType>(0)), 0u);
    EXPECT_EQ(service_.GetQueueSize(static_cast<SlotType>(15)), 0u);
}

TEST_F(MessageQueueServiceTest, GetQueueSizeAfterExtract) {
    service_.AddMessageToQueue(SlotType::TX, MakeMessage());
    service_.AddMessageToQueue(SlotType::TX, MakeMessage());
    service_.ExtractMessageOfType(SlotType::TX);
    EXPECT_EQ(service_.GetQueueSize(SlotType::TX), 1u);
}

// ---- ClearAllQueues ----

TEST_F(MessageQueueServiceTest, ClearAllQueuesRemovesAllMessages) {
    service_.AddMessageToQueue(SlotType::TX, MakeMessage());
    service_.AddMessageToQueue(SlotType::RX, MakeMessage());
    service_.AddMessageToQueue(SlotType::DISCOVERY_TX, MakeMessage());
    service_.ClearAllQueues();
    EXPECT_EQ(service_.GetTotalMessageCount(), 0u);
    EXPECT_FALSE(service_.HasAnyMessages());
}

TEST_F(MessageQueueServiceTest, ClearAllQueuesOnEmpty) {
    service_.ClearAllQueues();  // Should not crash
    EXPECT_EQ(service_.GetTotalMessageCount(), 0u);
}

// ---- SetMaxQueueSize / GetMaxQueueSize ----

TEST_F(MessageQueueServiceTest, GetMaxQueueSizeDefault) {
    EXPECT_EQ(service_.GetMaxQueueSize(), 5u);
}

TEST_F(MessageQueueServiceTest, SetMaxQueueSizeUpdates) {
    service_.SetMaxQueueSize(3);
    EXPECT_EQ(service_.GetMaxQueueSize(), 3u);
}

TEST_F(MessageQueueServiceTest, SetMaxQueueSizeTruncatesExisting) {
    for (int i = 0; i < 4; ++i) {
        service_.AddMessageToQueue(SlotType::TX, MakeMessage());
    }
    EXPECT_EQ(service_.GetQueueSize(SlotType::TX), 4u);

    service_.SetMaxQueueSize(2);
    // Should keep only newest 2
    EXPECT_EQ(service_.GetQueueSize(SlotType::TX), 2u);
}

TEST_F(MessageQueueServiceTest, SetMaxQueueSizeZeroMeansUnlimited) {
    MessageQueueService unlimited(0);
    for (int i = 0; i < 20; ++i) {
        unlimited.AddMessageToQueue(SlotType::TX, MakeMessage());
    }
    EXPECT_EQ(unlimited.GetQueueSize(SlotType::TX), 20u);
}

TEST_F(MessageQueueServiceTest, SetMaxQueueSizeLargerNoTruncation) {
    service_.AddMessageToQueue(SlotType::TX, MakeMessage());
    service_.AddMessageToQueue(SlotType::TX, MakeMessage());
    service_.SetMaxQueueSize(10);  // Increase — no truncation
    EXPECT_EQ(service_.GetQueueSize(SlotType::TX), 2u);
}

// ---- ClearQueue ----

TEST_F(MessageQueueServiceTest, ClearQueueClearsOnlySpecifiedType) {
    service_.AddMessageToQueue(SlotType::TX, MakeMessage());
    service_.AddMessageToQueue(SlotType::RX, MakeMessage());
    service_.ClearQueue(SlotType::TX);
    EXPECT_TRUE(service_.IsQueueEmpty(SlotType::TX));
    EXPECT_FALSE(service_.IsQueueEmpty(SlotType::RX));
}

TEST_F(MessageQueueServiceTest, ClearQueueInvalidTypeIsNoOp) {
    service_.AddMessageToQueue(SlotType::TX, MakeMessage());
    service_.ClearQueue(static_cast<SlotType>(0));   // invalid
    service_.ClearQueue(static_cast<SlotType>(10));  // invalid
    EXPECT_EQ(service_.GetQueueSize(SlotType::TX), 1u);
}

// ---- HasAnyMessages ----

TEST_F(MessageQueueServiceTest, HasAnyMessagesEmptyReturnsFalse) {
    EXPECT_FALSE(service_.HasAnyMessages());
}

TEST_F(MessageQueueServiceTest, HasAnyMessagesAfterAddReturnsTrue) {
    service_.AddMessageToQueue(SlotType::CONTROL_RX, MakeMessage());
    EXPECT_TRUE(service_.HasAnyMessages());
}

TEST_F(MessageQueueServiceTest, HasAnyMessagesAfterClearReturnsFalse) {
    service_.AddMessageToQueue(SlotType::TX, MakeMessage());
    service_.ClearAllQueues();
    EXPECT_FALSE(service_.HasAnyMessages());
}

// ---- GetTotalMessageCount ----

TEST_F(MessageQueueServiceTest, GetTotalMessageCountAcrossQueues) {
    service_.AddMessageToQueue(SlotType::TX, MakeMessage());
    service_.AddMessageToQueue(SlotType::TX, MakeMessage());
    service_.AddMessageToQueue(SlotType::RX, MakeMessage());
    service_.AddMessageToQueue(SlotType::SYNC_BEACON_RX, MakeMessage());
    EXPECT_EQ(service_.GetTotalMessageCount(), 4u);
}

TEST_F(MessageQueueServiceTest, GetTotalMessageCountEmptyIsZero) {
    EXPECT_EQ(service_.GetTotalMessageCount(), 0u);
}

// ---- HasMessage ----

TEST_F(MessageQueueServiceTest, HasMessageFindsExisting) {
    service_.AddMessageToQueue(SlotType::TX, MakeMessage(MessageType::PING));
    EXPECT_TRUE(service_.HasMessage(MessageType::PING));
}

TEST_F(MessageQueueServiceTest, HasMessageReturnsFalseForMissing) {
    service_.AddMessageToQueue(SlotType::TX, MakeMessage(MessageType::PING));
    EXPECT_FALSE(service_.HasMessage(MessageType::DATA));
}

TEST_F(MessageQueueServiceTest, HasMessageEmptyQueueReturnsFalse) {
    EXPECT_FALSE(service_.HasMessage(MessageType::PING));
}

TEST_F(MessageQueueServiceTest, HasMessageAcrossMultipleQueues) {
    service_.AddMessageToQueue(SlotType::TX, MakeMessage(MessageType::PING));
    service_.AddMessageToQueue(SlotType::RX, MakeMessage(MessageType::DATA));
    EXPECT_TRUE(service_.HasMessage(MessageType::DATA));
}

// ---- RemoveMessage ----

TEST_F(MessageQueueServiceTest, RemoveMessageFoundReturnsTrue) {
    service_.AddMessageToQueue(SlotType::TX, MakeMessage(MessageType::PING));
    EXPECT_TRUE(service_.RemoveMessage(MessageType::PING));
    EXPECT_TRUE(service_.IsQueueEmpty(SlotType::TX));
}

TEST_F(MessageQueueServiceTest, RemoveMessageNotFoundReturnsFalse) {
    service_.AddMessageToQueue(SlotType::TX, MakeMessage(MessageType::PING));
    EXPECT_FALSE(service_.RemoveMessage(MessageType::DATA));
    EXPECT_EQ(service_.GetQueueSize(SlotType::TX), 1u);
}

TEST_F(MessageQueueServiceTest, RemoveMessageFromSecondQueue) {
    service_.AddMessageToQueue(SlotType::TX, MakeMessage(MessageType::PING));
    service_.AddMessageToQueue(SlotType::RX, MakeMessage(MessageType::DATA));
    EXPECT_TRUE(service_.RemoveMessage(MessageType::DATA));
    EXPECT_EQ(service_.GetTotalMessageCount(), 1u);
    EXPECT_FALSE(service_.IsQueueEmpty(SlotType::TX));
}

TEST_F(MessageQueueServiceTest, RemoveMessageOnEmptyQueueReturnsFalse) {
    EXPECT_FALSE(service_.RemoveMessage(MessageType::PING));
}

// ---- Default constructor ----

TEST_F(MessageQueueServiceTest, DefaultConstructorMaxSizeTwenty) {
    MessageQueueService svc;
    EXPECT_EQ(svc.GetMaxQueueSize(), 20u);
    EXPECT_FALSE(svc.HasAnyMessages());
}

// ---- All slot types coverage ----

TEST_F(MessageQueueServiceTest, DiscoveryTXSlotType) {
    service_.AddMessageToQueue(SlotType::DISCOVERY_TX, MakeMessage());
    EXPECT_EQ(service_.GetQueueSize(SlotType::DISCOVERY_TX), 1u);
}

TEST_F(MessageQueueServiceTest, ControlRXSlotType) {
    service_.AddMessageToQueue(SlotType::CONTROL_RX, MakeMessage());
    EXPECT_EQ(service_.GetQueueSize(SlotType::CONTROL_RX), 1u);
}

TEST_F(MessageQueueServiceTest, SyncBeaconTXSlotType) {
    service_.AddMessageToQueue(SlotType::SYNC_BEACON_TX, MakeMessage());
    EXPECT_EQ(service_.GetQueueSize(SlotType::SYNC_BEACON_TX), 1u);
}

}  // namespace test
}  // namespace lora_mesh
}  // namespace protocols
}  // namespace loramesher
