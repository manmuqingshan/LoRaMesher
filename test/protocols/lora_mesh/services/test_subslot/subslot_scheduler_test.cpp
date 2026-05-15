/**
 * @file subslot_scheduler_test.cpp
 * @brief Unit tests for SubslotScheduler utility class
 */

#include <gtest/gtest.h>

#include "protocols/lora_mesh/services/subslot_scheduler.hpp"

namespace loramesher {
namespace protocols {
namespace lora_mesh {
namespace {

using SlotType = types::protocols::lora_mesh::SlotAllocation::SlotType;

// ============================================================================
// ComputeTiming - Basic Functionality
// ============================================================================

TEST(SubslotSchedulerTest, ComputeTimingHopBasedSubslot0) {
    SubslotConfig config;
    config.num_subslots = 5;
    config.guard_time_ms = 10;
    config.strategy = SubslotAssignment::HOP_BASED;

    // Network Manager at hop 0
    auto timing = SubslotScheduler::ComputeTiming(1000, config, 0);

    EXPECT_TRUE(timing.is_valid);
    EXPECT_EQ(timing.assigned_subslot, 0);
    // tx_start = 0 * subslot_duration + guard_time = guard_time
    EXPECT_EQ(timing.tx_start_offset_ms, 10);
}

TEST(SubslotSchedulerTest, ComputeTimingHopBasedSubslot1) {
    SubslotConfig config;
    config.num_subslots = 5;
    config.guard_time_ms = 10;
    config.strategy = SubslotAssignment::HOP_BASED;

    // Node at hop 1
    auto timing = SubslotScheduler::ComputeTiming(1000, config, 1);

    EXPECT_TRUE(timing.is_valid);
    EXPECT_EQ(timing.assigned_subslot, 1);
    // Each subslot = guard(10) + tx_window
    // Total guard = 5 * 10 = 50 ms
    // Trailing guard = min(30, 1000/10) = 30 ms
    // Available TX = 1000 - 50 - 30 = 920 ms
    // tx_window = 920 / 5 = 184 ms
    // subslot_duration = 10 + 184 = 194 ms
    // tx_start = 1 * 194 + 10 = 204 ms
    EXPECT_EQ(timing.tx_window_ms, 184u);
    EXPECT_EQ(timing.subslot_duration_ms, 194u);
    EXPECT_EQ(timing.tx_start_offset_ms, 204u);
}

TEST(SubslotSchedulerTest, ComputeTimingAddressModulo) {
    SubslotConfig config;
    config.num_subslots = 4;
    config.guard_time_ms = 10;
    config.strategy = SubslotAssignment::ADDRESS_MODULO;

    // Node address 0x1234 = 4660, 4660 % 4 = 0
    auto timing0 = SubslotScheduler::ComputeTiming(1000, config, 0x1234);
    EXPECT_TRUE(timing0.is_valid);
    EXPECT_EQ(timing0.assigned_subslot, 0);

    // Node address 0x1235 = 4661, 4661 % 4 = 1
    auto timing1 = SubslotScheduler::ComputeTiming(1000, config, 0x1235);
    EXPECT_TRUE(timing1.is_valid);
    EXPECT_EQ(timing1.assigned_subslot, 1);

    // Node address 0x1237 = 4663, 4663 % 4 = 3
    auto timing3 = SubslotScheduler::ComputeTiming(1000, config, 0x1237);
    EXPECT_TRUE(timing3.is_valid);
    EXPECT_EQ(timing3.assigned_subslot, 3);
}

TEST(SubslotSchedulerTest, DifferentSubslotsDoNotOverlap) {
    SubslotConfig config;
    config.num_subslots = 5;
    config.guard_time_ms = 10;
    config.strategy = SubslotAssignment::HOP_BASED;

    for (uint16_t i = 0; i < config.num_subslots; ++i) {
        auto timing_i = SubslotScheduler::ComputeTiming(1000, config, i);
        ASSERT_TRUE(timing_i.is_valid);

        uint32_t end_i = timing_i.tx_start_offset_ms + timing_i.tx_window_ms;

        for (uint16_t j = i + 1; j < config.num_subslots; ++j) {
            auto timing_j = SubslotScheduler::ComputeTiming(1000, config, j);
            ASSERT_TRUE(timing_j.is_valid);

            // j starts after i ends (with guard)
            EXPECT_GT(timing_j.tx_start_offset_ms, end_i)
                << "Subslot " << j << " overlaps with subslot " << i;
        }
    }
}

TEST(SubslotSchedulerTest, AllSubslotsFitWithinSlot) {
    SubslotConfig config;
    config.num_subslots = 5;
    config.guard_time_ms = 10;
    config.strategy = SubslotAssignment::HOP_BASED;

    uint32_t slot_duration = 1000;

    for (uint16_t i = 0; i < config.num_subslots; ++i) {
        auto timing = SubslotScheduler::ComputeTiming(slot_duration, config, i);
        ASSERT_TRUE(timing.is_valid);
        uint32_t end = timing.tx_start_offset_ms + timing.tx_window_ms;
        EXPECT_LE(end, slot_duration)
            << "Subslot " << i << " exceeds slot duration";
    }
}

TEST(SubslotSchedulerTest, HopBasedWrapsAround) {
    SubslotConfig config;
    config.num_subslots = 3;
    config.guard_time_ms = 10;
    config.strategy = SubslotAssignment::HOP_BASED;

    // Hop 3 wraps to subslot 0
    auto timing3 = SubslotScheduler::ComputeTiming(1000, config, 3);
    auto timing0 = SubslotScheduler::ComputeTiming(1000, config, 0);

    EXPECT_TRUE(timing3.is_valid);
    EXPECT_TRUE(timing0.is_valid);
    EXPECT_EQ(timing3.assigned_subslot, timing0.assigned_subslot);
    EXPECT_EQ(timing3.tx_start_offset_ms, timing0.tx_start_offset_ms);
}

// ============================================================================
// ComputeTiming - Edge Cases
// ============================================================================

TEST(SubslotSchedulerTest, ComputeTimingZeroSubslotsReturnsInvalid) {
    SubslotConfig config;
    config.num_subslots = 0;

    auto timing = SubslotScheduler::ComputeTiming(1000, config, 0);
    EXPECT_FALSE(timing.is_valid);
}

TEST(SubslotSchedulerTest, ComputeTimingZeroSlotDurationReturnsInvalid) {
    SubslotConfig config;
    config.num_subslots = 5;
    config.guard_time_ms = 10;

    auto timing = SubslotScheduler::ComputeTiming(0, config, 0);
    EXPECT_FALSE(timing.is_valid);
}

TEST(SubslotSchedulerTest, ComputeTimingGuardExceedsSlotReturnsInvalid) {
    SubslotConfig config;
    config.num_subslots = 5;
    config.guard_time_ms = 300;  // 5 * 300 = 1500 > 1000

    auto timing = SubslotScheduler::ComputeTiming(1000, config, 0);
    EXPECT_FALSE(timing.is_valid);
}

TEST(SubslotSchedulerTest, ComputeTimingSingleSubslot) {
    SubslotConfig config;
    config.num_subslots = 1;
    config.guard_time_ms = 10;
    config.strategy = SubslotAssignment::HOP_BASED;

    auto timing = SubslotScheduler::ComputeTiming(1000, config, 0);

    EXPECT_TRUE(timing.is_valid);
    EXPECT_EQ(timing.assigned_subslot, 0);
    EXPECT_EQ(timing.tx_start_offset_ms, 10u);
    // Trailing guard = min(30, 1000/10) = 30 ms; available = 1000 - 10 - 30 = 960 ms
    EXPECT_EQ(timing.tx_window_ms, 960u);
}

TEST(SubslotSchedulerTest, ComputeTimingLargeSlotDuration) {
    SubslotConfig config;
    config.num_subslots = 5;
    config.guard_time_ms = 50;
    config.strategy = SubslotAssignment::HOP_BASED;

    auto timing = SubslotScheduler::ComputeTiming(5000, config, 2);

    EXPECT_TRUE(timing.is_valid);
    EXPECT_EQ(timing.assigned_subslot, 2);
    // Total guard = 250 ms, trailing guard = min(30, 500) = 30 ms
    // Available TX = 5000 - 250 - 30 = 4720 ms, tx_window = 4720 / 5 = 944 ms
    // subslot_duration = 50 + 944 = 994 ms
    // tx_start = 2 * 994 + 50 = 2038 ms
    EXPECT_EQ(timing.tx_window_ms, 944u);
    EXPECT_EQ(timing.tx_start_offset_ms, 2038u);
}

// ============================================================================
// ValidateConfig
// ============================================================================

TEST(SubslotSchedulerTest, ValidateConfigSuccess) {
    SubslotConfig config;
    config.num_subslots = 5;
    config.guard_time_ms = 10;

    auto result = SubslotScheduler::ValidateConfig(1000, config, 100);
    EXPECT_TRUE(result.IsSuccess());
}

TEST(SubslotSchedulerTest, ValidateConfigZeroSubslots) {
    SubslotConfig config;
    config.num_subslots = 0;

    auto result = SubslotScheduler::ValidateConfig(1000, config, 100);
    EXPECT_FALSE(result.IsSuccess());
}

TEST(SubslotSchedulerTest, ValidateConfigZeroSlotDuration) {
    SubslotConfig config;
    config.num_subslots = 5;

    auto result = SubslotScheduler::ValidateConfig(0, config, 100);
    EXPECT_FALSE(result.IsSuccess());
}

TEST(SubslotSchedulerTest, ValidateConfigGuardExceedsSlot) {
    SubslotConfig config;
    config.num_subslots = 5;
    config.guard_time_ms = 300;  // 1500 ms > 1000 ms

    auto result = SubslotScheduler::ValidateConfig(1000, config, 100);
    EXPECT_FALSE(result.IsSuccess());
}

TEST(SubslotSchedulerTest, ValidateConfigToaExceedsTxWindow) {
    SubslotConfig config;
    config.num_subslots = 5;
    config.guard_time_ms = 10;

    // tx_window = (1000 - 50 - 30) / 5 = 184 ms
    // ToA = 200 ms > 184 ms
    auto result = SubslotScheduler::ValidateConfig(1000, config, 200);
    EXPECT_FALSE(result.IsSuccess());
}

TEST(SubslotSchedulerTest, ValidateConfigToaFitsExactly) {
    SubslotConfig config;
    config.num_subslots = 5;
    config.guard_time_ms = 10;

    // Trailing guard = 30 ms; tx_window = (1000 - 50 - 30) / 5 = 184 ms
    // ToA = 184 ms <= 184 ms
    auto result = SubslotScheduler::ValidateConfig(1000, config, 184);
    EXPECT_TRUE(result.IsSuccess());
}

// ============================================================================
// IsSubslottedSlotType
// ============================================================================

TEST(SubslotSchedulerTest, IsSubslottedSlotTypeSyncBeaconTx) {
    EXPECT_TRUE(
        SubslotScheduler::IsSubslottedSlotType(SlotType::SYNC_BEACON_TX));
}

TEST(SubslotSchedulerTest, IsSubslottedSlotTypeDiscoveryRx) {
    EXPECT_TRUE(SubslotScheduler::IsSubslottedSlotType(SlotType::DISCOVERY_RX));
}

TEST(SubslotSchedulerTest, IsSubslottedSlotTypeNonSubslotted) {
    EXPECT_FALSE(SubslotScheduler::IsSubslottedSlotType(SlotType::TX));
    EXPECT_FALSE(SubslotScheduler::IsSubslottedSlotType(SlotType::RX));
    EXPECT_FALSE(SubslotScheduler::IsSubslottedSlotType(SlotType::SLEEP));
    EXPECT_FALSE(SubslotScheduler::IsSubslottedSlotType(SlotType::CONTROL_RX));
    EXPECT_FALSE(SubslotScheduler::IsSubslottedSlotType(SlotType::CONTROL_TX));
    EXPECT_FALSE(
        SubslotScheduler::IsSubslottedSlotType(SlotType::SYNC_BEACON_RX));
}

// ============================================================================
// Consistency Checks
// ============================================================================

TEST(SubslotSchedulerTest, ConsistentTimingForSameInput) {
    SubslotConfig config;
    config.num_subslots = 5;
    config.guard_time_ms = 10;
    config.strategy = SubslotAssignment::HOP_BASED;

    auto t1 = SubslotScheduler::ComputeTiming(1000, config, 2);
    auto t2 = SubslotScheduler::ComputeTiming(1000, config, 2);

    EXPECT_EQ(t1.assigned_subslot, t2.assigned_subslot);
    EXPECT_EQ(t1.tx_start_offset_ms, t2.tx_start_offset_ms);
    EXPECT_EQ(t1.tx_window_ms, t2.tx_window_ms);
    EXPECT_EQ(t1.subslot_duration_ms, t2.subslot_duration_ms);
}

TEST(SubslotSchedulerTest, DifferentNodesGetDifferentOffsets) {
    SubslotConfig config;
    config.num_subslots = 5;
    config.guard_time_ms = 10;
    config.strategy = SubslotAssignment::ADDRESS_MODULO;

    auto t0 =
        SubslotScheduler::ComputeTiming(1000, config, 100);  // 100 % 5 = 0
    auto t1 =
        SubslotScheduler::ComputeTiming(1000, config, 101);  // 101 % 5 = 1
    auto t2 =
        SubslotScheduler::ComputeTiming(1000, config, 102);  // 102 % 5 = 2

    EXPECT_NE(t0.tx_start_offset_ms, t1.tx_start_offset_ms);
    EXPECT_NE(t1.tx_start_offset_ms, t2.tx_start_offset_ms);
    EXPECT_NE(t0.tx_start_offset_ms, t2.tx_start_offset_ms);
}

}  // namespace
}  // namespace lora_mesh
}  // namespace protocols
}  // namespace loramesher
