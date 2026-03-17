/**
 * @file slot_allocation_test.cpp
 * @brief Unit tests for SlotAllocation class
 */

#include <gtest/gtest.h>

#include "types/protocols/lora_mesh/slot_allocation.hpp"
#include "utils/byte_operations.h"

namespace loramesher {
namespace types {
namespace protocols {
namespace lora_mesh {
namespace test {

/**
 * @brief Test fixture for SlotAllocation tests
 */
class SlotAllocationTest : public ::testing::Test {
   protected:
    AddressType target_address_ = 0x1234;  // Example target address

    void SetUp() override {
        // Create sample slot allocations for testing
        tx_slot_ =
            SlotAllocation(10, SlotAllocation::SlotType::TX, target_address_);
        rx_slot_ = SlotAllocation(11, SlotAllocation::SlotType::RX);
        sleep_slot_ = SlotAllocation(12, SlotAllocation::SlotType::SLEEP);
        discovery_tx_slot_ =
            SlotAllocation(13, SlotAllocation::SlotType::DISCOVERY_TX);
        control_rx_slot_ =
            SlotAllocation(14, SlotAllocation::SlotType::CONTROL_RX);
    }

    SlotAllocation tx_slot_;
    SlotAllocation rx_slot_;
    SlotAllocation sleep_slot_;
    SlotAllocation discovery_tx_slot_;
    SlotAllocation control_rx_slot_;
};

/**
 * @brief Test default constructor
 */
TEST_F(SlotAllocationTest, DefaultConstructor) {
    SlotAllocation default_slot;

    EXPECT_EQ(default_slot.slot_number, 0);
    EXPECT_EQ(default_slot.target_address, 0);
    EXPECT_EQ(default_slot.type, SlotAllocation::SlotType::SLEEP);
}

/**
 * @brief Test parameterized constructor
 */
TEST_F(SlotAllocationTest, ParameterizedConstructor) {
    EXPECT_EQ(tx_slot_.slot_number, 10);
    EXPECT_EQ(tx_slot_.type, SlotAllocation::SlotType::TX);
    EXPECT_EQ(tx_slot_.target_address, 0x1234);

    EXPECT_EQ(rx_slot_.slot_number, 11);
    EXPECT_EQ(rx_slot_.type, SlotAllocation::SlotType::RX);
    EXPECT_EQ(rx_slot_.target_address, 0);  // Default target
}

/**
 * @brief Test IsTxSlot method
 */
TEST_F(SlotAllocationTest, IsTxSlot) {
    EXPECT_TRUE(tx_slot_.IsTxSlot());
    EXPECT_TRUE(discovery_tx_slot_.IsTxSlot());

    EXPECT_FALSE(rx_slot_.IsTxSlot());
    EXPECT_FALSE(control_rx_slot_.IsTxSlot());
    EXPECT_FALSE(sleep_slot_.IsTxSlot());
}

/**
 * @brief Test IsRxSlot method
 */
TEST_F(SlotAllocationTest, IsRxSlot) {
    EXPECT_TRUE(rx_slot_.IsRxSlot());
    EXPECT_TRUE(control_rx_slot_.IsRxSlot());

    EXPECT_FALSE(tx_slot_.IsRxSlot());
    EXPECT_FALSE(discovery_tx_slot_.IsRxSlot());
    EXPECT_FALSE(sleep_slot_.IsRxSlot());
}

/**
 * @brief Test IsControlSlot method
 */
TEST_F(SlotAllocationTest, IsControlSlot) {
    SlotAllocation control_tx_slot(20, SlotAllocation::SlotType::CONTROL_TX);

    EXPECT_TRUE(control_rx_slot_.IsControlSlot());
    EXPECT_TRUE(control_tx_slot.IsControlSlot());

    EXPECT_FALSE(tx_slot_.IsControlSlot());
    EXPECT_FALSE(rx_slot_.IsControlSlot());
    EXPECT_FALSE(discovery_tx_slot_.IsControlSlot());
    EXPECT_FALSE(sleep_slot_.IsControlSlot());
}

/**
 * @brief Test IsDiscoverySlot method
 */
TEST_F(SlotAllocationTest, IsDiscoverySlot) {
    SlotAllocation discovery_rx_slot(21,
                                     SlotAllocation::SlotType::DISCOVERY_RX);

    EXPECT_TRUE(discovery_tx_slot_.IsDiscoverySlot());
    EXPECT_TRUE(discovery_rx_slot.IsDiscoverySlot());

    EXPECT_FALSE(tx_slot_.IsDiscoverySlot());
    EXPECT_FALSE(rx_slot_.IsDiscoverySlot());
    EXPECT_FALSE(control_rx_slot_.IsDiscoverySlot());
    EXPECT_FALSE(sleep_slot_.IsDiscoverySlot());
}

/**
 * @brief Test GetTypeString method
 */
TEST_F(SlotAllocationTest, GetTypeString) {
    EXPECT_EQ(tx_slot_.GetTypeString(), "TX");
    EXPECT_EQ(rx_slot_.GetTypeString(), "RX");
    EXPECT_EQ(sleep_slot_.GetTypeString(), "SLEEP");
    EXPECT_EQ(discovery_tx_slot_.GetTypeString(), "DISCOVERY_TX");
    EXPECT_EQ(control_rx_slot_.GetTypeString(), "CONTROL_RX");
}

/**
 * @brief Test serialization and deserialization
 */
TEST_F(SlotAllocationTest, SerializationDeserialization) {
    // Serialize the slot allocation
    std::vector<uint8_t> buffer(SlotAllocation::SerializedSize());
    utils::ByteSerializer serializer(buffer);

    Result result = tx_slot_.Serialize(serializer);
    ASSERT_TRUE(result.IsSuccess());

    // Deserialize the slot allocation
    utils::ByteDeserializer deserializer(buffer);
    auto deserialized_slot = SlotAllocation::Deserialize(deserializer);

    ASSERT_TRUE(deserialized_slot.has_value());

    // Compare original and deserialized slots
    EXPECT_EQ(tx_slot_, *deserialized_slot);
    EXPECT_EQ(tx_slot_.slot_number, deserialized_slot->slot_number);
    EXPECT_EQ(tx_slot_.type, deserialized_slot->type);
    EXPECT_EQ(tx_slot_.target_address, deserialized_slot->target_address);
}

/**
 * @brief Test deserialization with invalid slot type
 */
TEST_F(SlotAllocationTest, DeserializationWithInvalidSlotType) {
    // Create buffer with invalid slot type
    std::vector<uint8_t> buffer = {
        0x10, 0x00,  // slot_number = 16
        0xFF,        // invalid slot type
        0x34, 0x12   // target_address = 0x1234
    };

    utils::ByteDeserializer deserializer(buffer);
    auto result = SlotAllocation::Deserialize(deserializer);

    EXPECT_FALSE(result.has_value());
}

/**
 * @brief Test equality operators
 */
TEST_F(SlotAllocationTest, EqualityOperators) {
    SlotAllocation equal_slot(10, SlotAllocation::SlotType::TX, 0x1234);
    SlotAllocation different_slot(10, SlotAllocation::SlotType::RX, 0x1234);

    EXPECT_TRUE(tx_slot_ == equal_slot);
    EXPECT_FALSE(tx_slot_ != equal_slot);

    EXPECT_FALSE(tx_slot_ == different_slot);
    EXPECT_TRUE(tx_slot_ != different_slot);
}

/**
 * @brief Test less than operator for sorting
 */
TEST_F(SlotAllocationTest, LessThanOperator) {
    SlotAllocation earlier_slot(5, SlotAllocation::SlotType::TX);
    SlotAllocation later_slot(15, SlotAllocation::SlotType::RX);

    EXPECT_TRUE(earlier_slot < tx_slot_);   // 5 < 10
    EXPECT_FALSE(tx_slot_ < earlier_slot);  // 10 > 5
    EXPECT_TRUE(tx_slot_ < later_slot);     // 10 < 15
    EXPECT_FALSE(later_slot < tx_slot_);    // 15 > 10
}

/**
 * @brief Test utility functions
 */
TEST_F(SlotAllocationTest, UtilityFunctions) {
    // Test SlotTypeToString
    EXPECT_EQ(slot_utils::SlotTypeToString(SlotAllocation::SlotType::TX), "TX");
    EXPECT_EQ(slot_utils::SlotTypeToString(SlotAllocation::SlotType::RX), "RX");
    EXPECT_EQ(slot_utils::SlotTypeToString(SlotAllocation::SlotType::SLEEP),
              "SLEEP");

    // Test StringToSlotType
    auto tx_type = slot_utils::StringToSlotType("TX");
    ASSERT_TRUE(tx_type.has_value());
    EXPECT_EQ(*tx_type, SlotAllocation::SlotType::TX);

    auto invalid_type = slot_utils::StringToSlotType("INVALID");
    EXPECT_FALSE(invalid_type.has_value());

    // Test IsValidSlotType
    EXPECT_TRUE(slot_utils::IsValidSlotType(SlotAllocation::SlotType::TX));
    EXPECT_TRUE(
        slot_utils::IsValidSlotType(SlotAllocation::SlotType::CONTROL_RX));

    // Test with cast from invalid uint8_t value
    SlotAllocation::SlotType invalid_cast =
        static_cast<SlotAllocation::SlotType>(0xFF);
    EXPECT_FALSE(slot_utils::IsValidSlotType(invalid_cast));
}

/**
 * @brief Test SerializedSize constant
 */
TEST_F(SlotAllocationTest, SerializedSize) {
    // Test that the constant matches actual serialized size
    std::vector<uint8_t> buffer(100);  // Large enough buffer
    utils::ByteSerializer serializer(buffer);

    tx_slot_.Serialize(serializer);
    size_t actual_size = serializer.getOffset();

    EXPECT_EQ(SlotAllocation::SerializedSize(), actual_size);
}

/**
 * @brief Test IsSyncBeaconSlot method
 */
TEST_F(SlotAllocationTest, IsSyncBeaconSlot) {
    SlotAllocation sync_tx(20, SlotAllocation::SlotType::SYNC_BEACON_TX);
    SlotAllocation sync_rx(21, SlotAllocation::SlotType::SYNC_BEACON_RX);

    EXPECT_TRUE(sync_tx.IsSyncBeaconSlot());
    EXPECT_TRUE(sync_rx.IsSyncBeaconSlot());

    EXPECT_FALSE(tx_slot_.IsSyncBeaconSlot());
    EXPECT_FALSE(rx_slot_.IsSyncBeaconSlot());
    EXPECT_FALSE(discovery_tx_slot_.IsSyncBeaconSlot());
    EXPECT_FALSE(control_rx_slot_.IsSyncBeaconSlot());
    EXPECT_FALSE(sleep_slot_.IsSyncBeaconSlot());
}

/**
 * @brief Test IsTxSlot for ALL tx slot types
 */
TEST_F(SlotAllocationTest, IsTxSlotAllTypes) {
    SlotAllocation control_tx(20, SlotAllocation::SlotType::CONTROL_TX);
    SlotAllocation sync_tx(21, SlotAllocation::SlotType::SYNC_BEACON_TX);

    EXPECT_TRUE(control_tx.IsTxSlot());
    EXPECT_TRUE(sync_tx.IsTxSlot());

    // Also verify non-tx types
    SlotAllocation discovery_rx(22, SlotAllocation::SlotType::DISCOVERY_RX);
    SlotAllocation sync_rx(23, SlotAllocation::SlotType::SYNC_BEACON_RX);
    EXPECT_FALSE(discovery_rx.IsTxSlot());
    EXPECT_FALSE(sync_rx.IsTxSlot());
}

/**
 * @brief Test IsRxSlot for ALL rx slot types
 */
TEST_F(SlotAllocationTest, IsRxSlotAllTypes) {
    SlotAllocation discovery_rx(20, SlotAllocation::SlotType::DISCOVERY_RX);
    SlotAllocation sync_rx(21, SlotAllocation::SlotType::SYNC_BEACON_RX);

    EXPECT_TRUE(discovery_rx.IsRxSlot());
    EXPECT_TRUE(sync_rx.IsRxSlot());
}

/**
 * @brief Test GetTypeString for all remaining slot types
 */
TEST_F(SlotAllocationTest, GetTypeStringAllTypes) {
    SlotAllocation control_tx(20, SlotAllocation::SlotType::CONTROL_TX);
    SlotAllocation discovery_rx(21, SlotAllocation::SlotType::DISCOVERY_RX);
    SlotAllocation sync_tx(22, SlotAllocation::SlotType::SYNC_BEACON_TX);
    SlotAllocation sync_rx(23, SlotAllocation::SlotType::SYNC_BEACON_RX);

    EXPECT_EQ(control_tx.GetTypeString(), "CONTROL_TX");
    EXPECT_EQ(discovery_rx.GetTypeString(), "DISCOVERY_RX");
    EXPECT_EQ(sync_tx.GetTypeString(), "SYNC_BEACON_TX");
    EXPECT_EQ(sync_rx.GetTypeString(), "SYNC_BEACON_RX");
}

/**
 * @brief Test StringToSlotType for all valid type strings
 */
TEST_F(SlotAllocationTest, StringToSlotTypeAllValid) {
    struct TestCase {
        std::string str;
        SlotAllocation::SlotType expected;
    };

    TestCase cases[] = {
        {"TX", SlotAllocation::SlotType::TX},
        {"RX", SlotAllocation::SlotType::RX},
        {"SLEEP", SlotAllocation::SlotType::SLEEP},
        {"DISCOVERY_RX", SlotAllocation::SlotType::DISCOVERY_RX},
        {"DISCOVERY_TX", SlotAllocation::SlotType::DISCOVERY_TX},
        {"CONTROL_RX", SlotAllocation::SlotType::CONTROL_RX},
        {"CONTROL_TX", SlotAllocation::SlotType::CONTROL_TX},
        {"SYNC_BEACON_TX", SlotAllocation::SlotType::SYNC_BEACON_TX},
        {"SYNC_BEACON_RX", SlotAllocation::SlotType::SYNC_BEACON_RX},
    };

    for (const auto& tc : cases) {
        auto result = slot_utils::StringToSlotType(tc.str);
        ASSERT_TRUE(result.has_value()) << "Failed for: " << tc.str;
        EXPECT_EQ(*result, tc.expected) << "Wrong result for: " << tc.str;
    }
}

/**
 * @brief Test IsValidSlotType for all valid types
 */
TEST_F(SlotAllocationTest, IsValidSlotTypeAllTypes) {
    using ST = SlotAllocation::SlotType;
    EXPECT_TRUE(slot_utils::IsValidSlotType(ST::TX));
    EXPECT_TRUE(slot_utils::IsValidSlotType(ST::RX));
    EXPECT_TRUE(slot_utils::IsValidSlotType(ST::SLEEP));
    EXPECT_TRUE(slot_utils::IsValidSlotType(ST::DISCOVERY_RX));
    EXPECT_TRUE(slot_utils::IsValidSlotType(ST::DISCOVERY_TX));
    EXPECT_TRUE(slot_utils::IsValidSlotType(ST::CONTROL_RX));
    EXPECT_TRUE(slot_utils::IsValidSlotType(ST::CONTROL_TX));
    EXPECT_TRUE(slot_utils::IsValidSlotType(ST::SYNC_BEACON_TX));
    EXPECT_TRUE(slot_utils::IsValidSlotType(ST::SYNC_BEACON_RX));
    EXPECT_FALSE(slot_utils::IsValidSlotType(static_cast<ST>(0x00)));
    EXPECT_FALSE(slot_utils::IsValidSlotType(static_cast<ST>(0xAA)));
}

/**
 * @brief Test SerializeDeserialize round-trip for all slot types
 */
TEST_F(SlotAllocationTest, SerializeAllSlotTypes) {
    using ST = SlotAllocation::SlotType;
    ST types[] = {ST::TX,
                  ST::RX,
                  ST::SLEEP,
                  ST::DISCOVERY_RX,
                  ST::DISCOVERY_TX,
                  ST::CONTROL_RX,
                  ST::CONTROL_TX,
                  ST::SYNC_BEACON_TX,
                  ST::SYNC_BEACON_RX};

    for (auto t : types) {
        SlotAllocation original(7, t, 0x5678);
        std::vector<uint8_t> buf(SlotAllocation::SerializedSize());
        utils::ByteSerializer ser(buf);
        ASSERT_TRUE(original.Serialize(ser).IsSuccess());

        utils::ByteDeserializer deser(buf);
        auto result = SlotAllocation::Deserialize(deser);
        ASSERT_TRUE(result.has_value())
            << "Failed for type " << static_cast<int>(t);
        EXPECT_EQ(result->type, t);
        EXPECT_EQ(result->slot_number, 7u);
        EXPECT_EQ(result->target_address, 0x5678u);
    }
}

/**
 * @brief Test SlotTypeToString for all types
 */
TEST_F(SlotAllocationTest, SlotTypeToStringAllTypes) {
    using ST = SlotAllocation::SlotType;
    EXPECT_EQ(slot_utils::SlotTypeToString(ST::CONTROL_TX), "CONTROL_TX");
    EXPECT_EQ(slot_utils::SlotTypeToString(ST::DISCOVERY_RX), "DISCOVERY_RX");
    EXPECT_EQ(slot_utils::SlotTypeToString(ST::SYNC_BEACON_TX),
              "SYNC_BEACON_TX");
    EXPECT_EQ(slot_utils::SlotTypeToString(ST::SYNC_BEACON_RX),
              "SYNC_BEACON_RX");
}

}  // namespace test
}  // namespace lora_mesh
}  // namespace protocols
}  // namespace types
}  // namespace loramesher