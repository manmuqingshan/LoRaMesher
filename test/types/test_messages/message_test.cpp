// test/types/test_messages/message_test.cpp
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

#include "types/messages/base_message.hpp"

namespace loramesher {
namespace test {

class BaseMessageTest : public ::testing::Test {
   protected:
    // Common test data
    // Test constants
    static constexpr AddressType dest = 0x1234;
    static constexpr AddressType src = 0x5678;
    static const inline std::vector<uint8_t> payload{0x01, 0x02, 0x03};

    std::unique_ptr<BaseMessage> msg_ptr;

    void SetUp() override { CreateMessage(); }

    void CreateMessage() {
        auto opt_msg =
            BaseMessage::Create(dest, src, MessageType::PING, payload);
        ASSERT_TRUE(opt_msg.has_value()) << "Failed to create test message";
        msg_ptr = std::make_unique<BaseMessage>(*opt_msg);
    }
};

TEST_F(BaseMessageTest, SerializationTest) {
    // Given: A valid message
    ASSERT_TRUE(msg_ptr != nullptr);

    // When: Serializing the message
    std::optional<std::vector<uint8_t>> opt_serialized = msg_ptr->Serialize();
    ASSERT_TRUE(opt_serialized.has_value()) << "Serialization failed";

    const std::vector<uint8_t>& serialized = *opt_serialized;

    // Then: Check size and structure
    ASSERT_EQ(serialized.size(), BaseHeader::Size() + payload.size())
        << "Incorrect serialized size";

    // And: Verify header fields in memory
    const uint8_t* data = serialized.data();
    uint16_t stored_dest = (data[1] << 8) | data[0];
    uint16_t stored_src = (data[3] << 8) | data[2];

    EXPECT_EQ(stored_dest, dest) << "Incorrect destination in serialized data";
    EXPECT_EQ(stored_src, src) << "Incorrect source in serialized data";
    EXPECT_EQ(data[4], static_cast<uint8_t>(MessageType::PING))
        << "Incorrect message type in serialized data";
    EXPECT_EQ(data[5], payload.size())
        << "Incorrect payload size in serialized data";

    // And: Verify payload
    EXPECT_EQ(memcmp(data + BaseHeader::Size(), payload.data(), payload.size()),
              0)
        << "Payload mismatch in serialized data";
}

/**
 * @brief Test deserialization functionality and error cases
 */
TEST_F(BaseMessageTest, DeserializationTest) {
    // Given: A serialized message
    auto opt_serialized = msg_ptr->Serialize();
    ASSERT_TRUE(opt_serialized.has_value()) << "Failed to serialize message";

    // When: Deserializing the message
    auto opt_deserialized = BaseMessage::CreateFromSerialized(*opt_serialized);
    ASSERT_TRUE(opt_deserialized.has_value())
        << "Failed to deserialize message";

    BaseMessage deserialized_msg = std::move(*opt_deserialized);

    // Then: Verify header fields
    const auto& header = deserialized_msg.GetHeader();
    EXPECT_EQ(header.GetDestination(), dest)
        << "Incorrect deserialized destination";
    EXPECT_EQ(header.GetSource(), src) << "Incorrect deserialized source";
    EXPECT_EQ(header.GetType(), MessageType::PING)
        << "Incorrect deserialized message type";
    EXPECT_EQ(header.GetPayloadSize(), payload.size())
        << "Incorrect deserialized payload size";

    // And: Verify payload
    EXPECT_TRUE(std::ranges::equal(deserialized_msg.GetPayload(), payload))
        << "Incorrect deserialized payload";
}

/**
 * @brief Test deserialization error handling
 */
TEST_F(BaseMessageTest, DeserializationFailureTest) {
    // Test: Empty data
    {
        std::vector<uint8_t> empty_data;
        auto result = BaseMessage::CreateFromSerialized(empty_data);
        EXPECT_FALSE(result.has_value()) << "Should fail with empty data";
    }

    // Test: Incomplete header
    {
        std::vector<uint8_t> incomplete_data{0x01, 0x02, 0x03};
        auto result = BaseMessage::CreateFromSerialized(incomplete_data);
        EXPECT_FALSE(result.has_value())
            << "Should fail with incomplete header";
    }

    // Test: Invalid message type
    {
        auto opt_serialized = msg_ptr->Serialize();
        ASSERT_TRUE(opt_serialized.has_value());
        std::vector<uint8_t> invalid_type = *opt_serialized;
        invalid_type[4] = 0xFF;  // Invalid message type
        auto result = BaseMessage::CreateFromSerialized(invalid_type);
        EXPECT_FALSE(result.has_value())
            << "Should fail with invalid message type";
    }
}

// Test copy constructor
TEST_F(BaseMessageTest, CopyConstructorTest) {
    {
        BaseMessage msg = *msg_ptr;
        BaseMessage copy(msg);

        // Verify independent copies
        EXPECT_TRUE(
            std::ranges::equal(copy.GetPayload(), msg_ptr->GetPayload()));
        EXPECT_NE(copy.GetPayload().data(), msg_ptr->GetPayload().data());
    }
    // Both objects should be properly destroyed here
}

// Test copy assignment
TEST_F(BaseMessageTest, CopyAssignmentTest) {
    {
        std::optional<loramesher::BaseMessage> opt_copy = BaseMessage::Create(
            0x0000, 0x0000, MessageType::PING, std::vector<uint8_t>{0xFF});
        if (!opt_copy) {
            FAIL() << "Failed to create message";
        }

        BaseMessage copy = *opt_copy;
        BaseMessage msg = *msg_ptr;
        copy = msg;

        // Verify independent copies
        EXPECT_TRUE(
            std::ranges::equal(copy.GetPayload(), msg_ptr->GetPayload()));
        EXPECT_NE(copy.GetPayload().data(), msg_ptr->GetPayload().data());
    }
    // Copy should be destroyed, original should still be valid
    EXPECT_TRUE(std::ranges::equal(msg_ptr->GetPayload(), payload));
}

// Test move constructor
TEST_F(BaseMessageTest, MoveConstructorTest) {
    {
        BaseMessage moved(std::move(*msg_ptr));

        // Verify moved data
        EXPECT_TRUE(std::ranges::equal(moved.GetPayload(), payload));

        // Original should be in valid but unspecified state
        EXPECT_TRUE(msg_ptr->GetPayload().empty());
    }
}

// Test move assignment
TEST_F(BaseMessageTest, MoveAssignmentTest) {
    // Given: A source message with known payload
    BaseMessage source_msg = *msg_ptr;

    // Create target with different content
    auto opt_target = BaseMessage::Create(0x0000, 0x0000, MessageType::PING,
                                          std::vector<uint8_t>{0xFF});
    ASSERT_TRUE(opt_target.has_value()) << "Failed to create target message";
    BaseMessage target_msg = std::move(*opt_target);

    // Store the original payload for comparison
    std::vector<uint8_t> original_payload(source_msg.GetPayload().begin(),
                                          source_msg.GetPayload().end());

    // When: Moving source to target
    target_msg = std::move(source_msg);

    // Then: Target should have the original payload
    EXPECT_TRUE(std::ranges::equal(target_msg.GetPayload(), original_payload))
        << "Target payload doesn't match original";

    // And: Source should be empty but valid
    EXPECT_TRUE(source_msg.GetPayload().empty())
        << "Source message not empty after move";

    // And: Source and target should have different payload storage
    EXPECT_NE(source_msg.GetPayload().data(), target_msg.GetPayload().data())
        << "Source and target point to same storage after move";
}

// Test error safety
TEST_F(BaseMessageTest, CreateErrorTest) {
    const auto originalPayload = msg_ptr->GetPayload();

    // Force exception by creating message with invalid data
    std::vector<uint8_t> invalidPayload;
    invalidPayload.resize(std::numeric_limits<uint8_t>::max() + 1);
    std::optional<loramesher::BaseMessage> opt_msg =
        BaseMessage::Create(0, 0, MessageType::PING, invalidPayload);

    if (opt_msg) {
        FAIL() << "Expected optional to be empty";
    }

    // Original should remain unchanged
    EXPECT_TRUE(std::ranges::equal(msg_ptr->GetPayload(), originalPayload));
}

// Test chained operations
TEST_F(BaseMessageTest, ChainedOperationsTest) {
    std::vector<std::unique_ptr<BaseMessage>> messages;

    // Create and move messages in a chain
    for (int i = 0; i < 10; ++i) {
        auto opt_msg =
            BaseMessage::Create(dest, src, MessageType::PING, payload);
        if (!opt_msg) {
            FAIL() << "Failed to create message";
        }
        auto ptr_msg = std::make_unique<BaseMessage>(*opt_msg);

        messages.push_back(std::move(ptr_msg));
        EXPECT_TRUE(ptr_msg ==
                    nullptr);  // Original pointer should be null after move
    }

    // Verify all messages are valid
    for (const auto& msg : messages) {
        EXPECT_TRUE(std::ranges::equal(msg->GetPayload(), payload));
    }
}

// Test boundary conditions
TEST_F(BaseMessageTest, BoundaryConditionsTest) {
    // Empty payload
    {
        Result result = msg_ptr->SetMessage(dest, src, MessageType::PING, {});
        EXPECT_TRUE(result.IsSuccess());

        EXPECT_EQ(msg_ptr->GetPayload().size(), 0);
        EXPECT_EQ(msg_ptr->GetTotalSize(), BaseHeader::Size());
    }

    // Maximum size payload
    {
        uint8_t max_payload_size = std::numeric_limits<uint8_t>::max();
        std::vector<uint8_t> max_payload(max_payload_size);
        Result result =
            msg_ptr->SetMessage(dest, src, MessageType::PING, max_payload);
        EXPECT_TRUE(result.IsSuccess());
        EXPECT_EQ(msg_ptr->GetPayload().size(),
                  std::numeric_limits<uint8_t>::max());
    }
}

// Additional validation tests
TEST_F(BaseMessageTest, PayloadSizeValidationTest) {
    // Test exactly at the limit
    std::vector<uint8_t> maxPayload(BaseMessage::kMaxPayloadSize, 0xFF);

    Result result =
        msg_ptr->SetMessage(dest, src, MessageType::PING, maxPayload);
    EXPECT_TRUE(result.IsSuccess());

    // Test one byte over the limit
    std::vector<uint8_t> tooLargePayload(BaseMessage::kMaxPayloadSize + 1,
                                         0xFF);
    result = msg_ptr->SetMessage(dest, src, MessageType::PING, tooLargePayload);
    EXPECT_FALSE(result.IsSuccess());
}

TEST_F(BaseMessageTest, MessageTypeValidationTest) {
    // Test invalid message type
    Result result =
        msg_ptr->SetMessage(dest, src, static_cast<MessageType>(0xFF), payload);
    EXPECT_FALSE(result.IsSuccess());
}

// ---- MessageType helper function tests ----

TEST_F(BaseMessageTest, MessageTypeIsDataMessage) {
    // DATA (0x11) has high nibble 0x10 == DATA_MSG
    EXPECT_TRUE(message_type::IsDataMessage(MessageType::DATA));

    // JOIN_REQUEST (0x42) has high nibble 0x40 == SYSTEM_MSG, not DATA_MSG
    EXPECT_FALSE(message_type::IsDataMessage(MessageType::JOIN_REQUEST));

    // PING (0x23) is a control message, not data
    EXPECT_FALSE(message_type::IsDataMessage(MessageType::PING));
}

TEST_F(BaseMessageTest, MessageTypeIsControlMessage) {
    // PING (0x23) has high nibble 0x20 == CONTROL_MSG
    EXPECT_TRUE(message_type::IsControlMessage(MessageType::PING));

    // ACK (0x21) is also a control message
    EXPECT_TRUE(message_type::IsControlMessage(MessageType::ACK));

    // PONG (0x24) is a control message
    EXPECT_TRUE(message_type::IsControlMessage(MessageType::PONG));

    // DATA (0x11) is not a control message
    EXPECT_FALSE(message_type::IsControlMessage(MessageType::DATA));
}

TEST_F(BaseMessageTest, MessageTypeIsRoutingMessage) {
    // HELLO (0x31) has high nibble 0x30 == ROUTING_MSG
    EXPECT_TRUE(message_type::IsRoutingMessage(MessageType::HELLO));

    // ROUTE_TABLE (0x32) is also a routing message
    EXPECT_TRUE(message_type::IsRoutingMessage(MessageType::ROUTE_TABLE));

    // SYNC_BEACON (0x46) is a system message, not routing
    EXPECT_FALSE(message_type::IsRoutingMessage(MessageType::SYNC_BEACON));
}

TEST_F(BaseMessageTest, MessageTypeIsSystemMessage) {
    // SYNC_BEACON (0x46) has high nibble 0x40 == SYSTEM_MSG
    EXPECT_TRUE(message_type::IsSystemMessage(MessageType::SYNC_BEACON));

    // JOIN_REQUEST (0x42) is a system message
    EXPECT_TRUE(message_type::IsSystemMessage(MessageType::JOIN_REQUEST));

    // NM_CLAIM (0x47) is a system message
    EXPECT_TRUE(message_type::IsSystemMessage(MessageType::NM_CLAIM));

    // HELLO (0x31) is a routing message, not system
    EXPECT_FALSE(message_type::IsSystemMessage(MessageType::HELLO));
}

TEST_F(BaseMessageTest, MessageTypeCreateType) {
    // Combine DATA_MSG (0x10) with subtype 0x01 -> should equal DATA (0x11)
    MessageType created = message_type::CreateType(
        MessageType::DATA_MSG, static_cast<MessageType>(0x01));
    EXPECT_EQ(created, MessageType::DATA);

    // Combine CONTROL_MSG (0x20) with subtype 0x03 -> should equal PING (0x23)
    MessageType created_ping = message_type::CreateType(
        MessageType::CONTROL_MSG, static_cast<MessageType>(0x03));
    EXPECT_EQ(created_ping, MessageType::PING);

    // Verify the main type and subtype are preserved correctly
    EXPECT_EQ(message_type::GetMainType(created), MessageType::DATA_MSG);
    EXPECT_EQ(static_cast<uint8_t>(message_type::GetSubtype(created)), 0x01u);
}

TEST_F(BaseMessageTest, BaseHeaderSerializeToVector) {
    ASSERT_TRUE(msg_ptr != nullptr);

    // Serialize the full message first to get a valid byte buffer
    auto opt_serialized = msg_ptr->Serialize();
    ASSERT_TRUE(opt_serialized.has_value());

    // Reconstruct a BaseMessage from the serialized bytes
    auto opt_msg = BaseMessage::CreateFromSerialized(*opt_serialized);
    ASSERT_TRUE(opt_msg.has_value());

    // Get the header and call the no-arg Serialize() that returns std::vector
    const BaseHeader& header = opt_msg->GetHeader();
    std::vector<uint8_t> header_bytes = header.Serialize();

    // Verify the returned vector has the expected size
    EXPECT_EQ(header_bytes.size(), BaseHeader::Size());

    // Verify the destination and source are correctly serialized (little-endian)
    ASSERT_GE(header_bytes.size(), 4u);
    uint16_t stored_dest = static_cast<uint16_t>(header_bytes[0]) |
                           (static_cast<uint16_t>(header_bytes[1]) << 8);
    uint16_t stored_src = static_cast<uint16_t>(header_bytes[2]) |
                          (static_cast<uint16_t>(header_bytes[3]) << 8);
    EXPECT_EQ(stored_dest, dest);
    EXPECT_EQ(stored_src, src);
}

}  // namespace test
}  // namespace loramesher