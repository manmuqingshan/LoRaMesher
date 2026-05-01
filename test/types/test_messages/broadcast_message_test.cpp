/**
 * @file broadcast_message_test.cpp
 * @brief Unit tests for BroadcastMessage class
 */

#include <gtest/gtest.h>

#include "types/messages/loramesher/broadcast_message.hpp"

namespace loramesher {
namespace test {

class BroadcastMessageTest : public ::testing::Test {
   protected:
    static constexpr AddressType src = 0x5678;
    static constexpr uint8_t ttl = 5;
    static constexpr uint8_t seq_num = 42;
    std::vector<uint8_t> payload{0x11, 0x22, 0x33, 0x44};

    std::unique_ptr<BroadcastMessage> msg_ptr;

    void SetUp() override {
        auto opt_msg = BroadcastMessage::Create(src, ttl, seq_num, payload);
        ASSERT_TRUE(opt_msg.has_value());
        msg_ptr = std::make_unique<BroadcastMessage>(*opt_msg);
    }
};

TEST_F(BroadcastMessageTest, CreateFromBaseMessageSucceeds) {
    ASSERT_TRUE(msg_ptr != nullptr);

    BaseMessage base = msg_ptr->ToBaseMessage();
    ASSERT_EQ(base.GetType(), MessageType::DATA_BROADCAST);

    auto result = BroadcastMessage::CreateFromBaseMessage(base);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->GetSource(), src);
    EXPECT_EQ(result->GetTTL(), ttl);
    EXPECT_EQ(result->GetSeqNum(), seq_num);
    EXPECT_EQ(result->GetPayload(), payload);
}

TEST_F(BroadcastMessageTest, CreateFromBaseMessageWrongTypeReturnsNullopt) {
    std::array<uint8_t, 4> data{0x01, 0x02, 0x03, 0x04};
    BaseMessage wrong(0xFFFF, src, MessageType::DATA,
                      std::span<const uint8_t>(data));

    auto result = BroadcastMessage::CreateFromBaseMessage(wrong);
    EXPECT_FALSE(result.has_value());
}

TEST_F(BroadcastMessageTest, CreatePayloadTooLargeReturnsNullopt) {
    std::vector<uint8_t> oversized(BaseMessage::kMaxPayloadSize -
                                       BroadcastMessage::kBroadcastFieldsSize +
                                       1,
                                   0xAA);
    auto result = BroadcastMessage::Create(src, ttl, seq_num, oversized);
    EXPECT_FALSE(result.has_value());
}

TEST_F(BroadcastMessageTest, CreateAtPayloadSizeBoundary) {
    std::vector<uint8_t> at_limit(
        BaseMessage::kMaxPayloadSize - BroadcastMessage::kBroadcastFieldsSize,
        0xBB);
    auto result = BroadcastMessage::Create(src, ttl, seq_num, at_limit);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->GetPayload().size(), at_limit.size());
}

TEST_F(BroadcastMessageTest, GetDestinationIsBroadcastAddress) {
    EXPECT_EQ(msg_ptr->GetDestination(), kBroadcastAddress);
}

TEST_F(BroadcastMessageTest, GetTotalSizeReflectsPayload) {
    EXPECT_EQ(msg_ptr->GetTotalSize(),
              BaseHeader::Size() + BroadcastMessage::kBroadcastFieldsSize +
                  payload.size());
}

TEST_F(BroadcastMessageTest, CreateForwardedDecrementsTTL) {
    auto forwarded = BroadcastMessage::CreateForwarded(*msg_ptr);
    ASSERT_TRUE(forwarded.has_value());
    EXPECT_EQ(forwarded->GetTTL(), ttl - 1);
    EXPECT_EQ(forwarded->GetSource(), src);
    EXPECT_EQ(forwarded->GetSeqNum(), seq_num);
    EXPECT_EQ(forwarded->GetPayload(), payload);
}

TEST_F(BroadcastMessageTest, CreateForwardedAtTTLOneReturnsNullopt) {
    auto last_hop = BroadcastMessage::Create(src, 1, seq_num, payload);
    ASSERT_TRUE(last_hop.has_value());
    auto forwarded = BroadcastMessage::CreateForwarded(*last_hop);
    EXPECT_FALSE(forwarded.has_value());
}

TEST_F(BroadcastMessageTest, CreateForwardedAtTTLZeroReturnsNullopt) {
    auto expired = BroadcastMessage::Create(src, 0, seq_num, payload);
    ASSERT_TRUE(expired.has_value());
    auto forwarded = BroadcastMessage::CreateForwarded(*expired);
    EXPECT_FALSE(forwarded.has_value());
}

TEST_F(BroadcastMessageTest, SerializeProducesExpectedSize) {
    auto serialized = msg_ptr->Serialize();
    ASSERT_TRUE(serialized.has_value());
    EXPECT_EQ(serialized->size(), msg_ptr->GetTotalSize());
}

TEST_F(BroadcastMessageTest, SerializeWithEmptyPayloadIsValid) {
    auto empty_msg =
        BroadcastMessage::Create(src, ttl, seq_num, std::span<const uint8_t>{});
    ASSERT_TRUE(empty_msg.has_value());
    auto serialized = empty_msg->Serialize();
    ASSERT_TRUE(serialized.has_value());
    EXPECT_EQ(serialized->size(),
              BaseHeader::Size() + BroadcastMessage::kBroadcastFieldsSize);
}

TEST_F(BroadcastMessageTest, CreateFromSerializedRoundTrip) {
    auto serialized = msg_ptr->Serialize();
    ASSERT_TRUE(serialized.has_value());

    auto round_trip = BroadcastMessage::CreateFromSerialized(*serialized);
    ASSERT_TRUE(round_trip.has_value());
    EXPECT_EQ(round_trip->GetSource(), src);
    EXPECT_EQ(round_trip->GetTTL(), ttl);
    EXPECT_EQ(round_trip->GetSeqNum(), seq_num);
    EXPECT_EQ(round_trip->GetPayload(), payload);
}

TEST_F(BroadcastMessageTest, CreateFromSerializedWithEmptyPayloadRoundTrip) {
    auto empty_msg =
        BroadcastMessage::Create(src, ttl, seq_num, std::span<const uint8_t>{});
    ASSERT_TRUE(empty_msg.has_value());
    auto serialized = empty_msg->Serialize();
    ASSERT_TRUE(serialized.has_value());

    auto round_trip = BroadcastMessage::CreateFromSerialized(*serialized);
    ASSERT_TRUE(round_trip.has_value());
    EXPECT_TRUE(round_trip->GetPayload().empty());
}

TEST_F(BroadcastMessageTest, CreateFromSerializedTooSmallReturnsNullopt) {
    std::vector<uint8_t> too_small(BaseHeader::Size() +
                                   BroadcastMessage::kBroadcastFieldsSize - 1);
    auto result = BroadcastMessage::CreateFromSerialized(too_small);
    EXPECT_FALSE(result.has_value());
}

TEST_F(BroadcastMessageTest, CreateFromSerializedWrongTypeReturnsNullopt) {
    std::vector<uint8_t> buffer(BaseHeader::Size() +
                                BroadcastMessage::kBroadcastFieldsSize);
    utils::ByteSerializer serializer(buffer);
    BaseHeader header(
        kBroadcastAddress, src, MessageType::DATA,
        static_cast<uint8_t>(BroadcastMessage::kBroadcastFieldsSize));
    header.Serialize(serializer);
    serializer.WriteUint16(kBroadcastAddress);
    serializer.WriteUint8(ttl);
    serializer.WriteUint8(seq_num);

    auto result = BroadcastMessage::CreateFromSerialized(buffer);
    EXPECT_FALSE(result.has_value());
}

TEST_F(BroadcastMessageTest,
       CreateFromBaseMessagePayloadTooSmallReturnsNullopt) {
    std::array<uint8_t, 2> truncated{0xFF, 0xFF};
    BaseMessage too_small(kBroadcastAddress, src, MessageType::DATA_BROADCAST,
                          std::span<const uint8_t>(truncated));

    auto result = BroadcastMessage::CreateFromBaseMessage(too_small);
    EXPECT_FALSE(result.has_value());
}

TEST_F(BroadcastMessageTest, ToBaseMessageEmptyPayloadHasOnlyFields) {
    auto empty_msg =
        BroadcastMessage::Create(src, ttl, seq_num, std::span<const uint8_t>{});
    ASSERT_TRUE(empty_msg.has_value());
    BaseMessage base = empty_msg->ToBaseMessage();
    EXPECT_EQ(base.GetType(), MessageType::DATA_BROADCAST);
    EXPECT_EQ(base.GetPayload().size(), BroadcastMessage::kBroadcastFieldsSize);
}

}  // namespace test
}  // namespace loramesher
