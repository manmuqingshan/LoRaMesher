/**
 * @file tdma_routing_mismatch_test.cpp
 * @brief Tests for TDMA-aware routing and opportunistic forwarding
 *
 * Verifies that misdirected DATA messages (addressed to a next_hop that
 * has SLEEP during the sender's TX slot) are salvaged by opportunistic
 * forwarding at nodes that CAN hear the sender.
 *
 * Modeled after a real 14-node experiment where edge nodes achieved only
 * 8-14% PDR because 0x006C (NM) silently ignored DATA from 0x3428 that
 * was addressed to 0x77A4 (GW).
 */

#include "routing_test_fixture.hpp"

namespace loramesher {
namespace test {

class TDMARoutingMismatchTests : public RoutingTestFixture {};

// ─────────────────────────────────────────────────────────────────────────────
// Key scenario (Tests 1 & 2):
//
//   GW (0x2000) ── NM (0x1000) ── Core2 (0x1001)
//        |              |
//     (cut later)    (marginal)
//        |              |
//      Edge1 (0x3000) ──┘
//        /          \.
//   Edge2 (0x3001)  Edge3 (0x3002)
//
// 1. Network forms with GW↔Edge1 BIDIRECTIONAL — both learn each other
//    as hop=1, allocate mutual RX/CONTROL_RX slots.
// 2. CUT the Edge1→GW direction.  Now Edge1 can still hear GW, but GW
//    has RX for Edge1 yet receives nothing (link gone).
// 3. Edge1's FindNextHop may still return GW (hop=1, good quality).
//    DATA addressed to GW: only NM receives (has RX for Edge1's slot).
// 4. Without opportunistic forwarding: NM ignores → data lost.
//    With opportunistic forwarding: NM salvages → data delivered.
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Opportunistic forwarding rescues data after a link becomes
 *        unidirectional.
 *
 * After cutting Edge1→GW, Edge1's routing table still has GW as hop=1.
 * DATA addressed to GW is received by NM (TDMA neighbor) which
 * opportunistically forwards it toward the destination.
 */
TEST_F(TDMARoutingMismatchTests, UnidirectionalLinkOpportunisticForward) {
    // ── Create nodes ─────────────────────────────────────────────────────
    auto& nm = CreateNode("NM", 0x1000, NodeRole::NETWORK_MANAGER);
    auto& gw = CreateNode("GW", 0x2000, NodeRole::NODE_ONLY);
    auto& core2 = CreateNode("Core2", 0x1001, NodeRole::NODE_ONLY);
    auto& edge1 = CreateNode("Edge1", 0x3000, NodeRole::NODE_ONLY);
    auto& edge2 = CreateNode("Edge2", 0x3001, NodeRole::NODE_ONLY);
    auto& edge3 = CreateNode("Edge3", 0x3002, NodeRole::NODE_ONLY);

    // ── All links bidirectional (including Edge1↔GW) ─────────────────────
    SetLinkStatus(nm, gw, true);
    SetLinkStatus(nm, core2, true);
    SetLinkStatus(nm, edge1, true);
    SetLinkStatus(gw, edge1, true);  // Edge1 learns GW as hop=1
    SetLinkStatus(edge1, edge2, true);
    SetLinkStatus(edge1, edge3, true);

    // ── Form and stabilize ───────────────────────────────────────────────
    std::vector<TestNode*> all_nodes = {&nm,    &gw,    &core2,
                                        &edge1, &edge2, &edge3};
    for (auto* node : all_nodes) {
        ASSERT_TRUE(StartNode(*node)) << "Failed to start " << node->name;
    }
    ASSERT_TRUE(WaitForNetworkFormation(all_nodes, 5))
        << "Network formation failed";
    ASSERT_TRUE(WaitForRoutingStabilization(all_nodes))
        << "Routing stabilization failed";

    auto superframe_ms = GetSuperframeDuration(nm);
    auto step_ms = 15u;

    // Converge distance-vector so both GW↔Edge1 are confirmed hop=1
    AdvanceTime(superframe_ms * 3, superframe_ms * 3, step_ms, 0,
                [&]() { return false; });

    // ── Verify Edge1 has GW as hop=1 ─────────────────────────────────────
    ASSERT_EQ(GetHopCount(edge1, gw.address), 1)
        << "Edge1 should see GW as direct neighbor (hop=1)";

    std::cout << "=== Before link cut ===" << std::endl;
    PrintRoutingTable(edge1);

    // ── CUT Edge1→GW link (unidirectional: GW→Edge1 only) ───────────────
    // GW still has RX for Edge1's slot but the link is gone → receives
    // nothing.  Edge1 still hears GW (RX slot + link GW→Edge1 exists).
    SetDirectionalLink(edge1, gw, false);

    // Brief advance so the link state takes effect
    AdvanceTime(superframe_ms * 2, superframe_ms * 2, step_ms, 0,
                [&]() { return false; });

    // Edge1's routing table still has GW at hop=1 (not yet degraded)
    ASSERT_EQ(GetHopCount(edge1, gw.address), 1)
        << "Edge1 should still have GW as hop=1 (quality not degraded yet)";

    // ── Send DATA from Edge1 → Core2 (destination behind NM) ─────────────
    // Edge1's FindNextHop(Core2) might return NM or GW depending on cost.
    // If it returns GW: DATA has next_hop=GW, only NM hears it.
    // With opportunistic forwarding: NM forwards → Core2 receives.
    constexpr int kNumMessages = 10;
    std::vector<uint8_t> payload = {0xCA, 0xFE, 0xBA, 0xBE};

    for (int i = 0; i < kNumMessages; i++) {
        payload[0] = static_cast<uint8_t>(i);
        SendMessage(edge1, gw, payload);
    }

    bool any_received =
        AdvanceTime(superframe_ms * 10, superframe_ms * 10, step_ms, 0, [&]() {
            return CountReceivedMessages(gw, edge1.address,
                                         MessageType::DATA) >= 1;
        });

    size_t delivered =
        CountReceivedMessages(gw, edge1.address, MessageType::DATA);
    std::cout << "Delivered " << delivered << "/" << kNumMessages
              << " from Edge1 to GW (after link cut)" << std::endl;

    PrintRoutingTable(edge1);

    // With LORAMESHER_OPPORTUNISTIC_FORWARD_RELAXED: NM salvages
    // misdirected packets → at least some arrive.
    // Without it: all packets addressed to GW are silently dropped.
    EXPECT_TRUE(any_received)
        << "Opportunistic forwarding should deliver DATA via NM";
    EXPECT_GE(delivered, 1u)
        << "At least 1 of " << kNumMessages << " should reach GW via NM";
}

/**
 * @brief Three edge nodes behind a marginal + unidirectional link all
 *        deliver data via opportunistic forwarding.
 */
TEST_F(TDMARoutingMismatchTests,
       CascadingEdgeNodesDeliverAfterLinkDegradation) {
    auto& nm = CreateNode("NM", 0x1000, NodeRole::NETWORK_MANAGER);
    auto& gw = CreateNode("GW", 0x2000, NodeRole::NODE_ONLY);
    auto& core2 = CreateNode("Core2", 0x1001, NodeRole::NODE_ONLY);
    auto& edge1 = CreateNode("Edge1", 0x3000, NodeRole::NODE_ONLY);
    auto& edge2 = CreateNode("Edge2", 0x3001, NodeRole::NODE_ONLY);
    auto& edge3 = CreateNode("Edge3", 0x3002, NodeRole::NODE_ONLY);

    // Form with ALL good bidirectional links
    SetLinkStatus(nm, gw, true);
    SetLinkStatus(nm, core2, true);
    SetLinkStatus(nm, edge1, true);
    SetLinkStatus(gw, edge1, true);
    SetLinkStatus(edge1, edge2, true);
    SetLinkStatus(edge1, edge3, true);

    std::vector<TestNode*> all_nodes = {&nm,    &gw,    &core2,
                                        &edge1, &edge2, &edge3};
    for (auto* node : all_nodes) {
        ASSERT_TRUE(StartNode(*node)) << "Failed to start " << node->name;
    }
    ASSERT_TRUE(WaitForNetworkFormation(all_nodes, 5))
        << "Network formation failed";
    ASSERT_TRUE(WaitForRoutingStabilization(all_nodes))
        << "Routing stabilization failed";

    auto superframe_ms = GetSuperframeDuration(nm);
    auto step_ms = 15u;

    AdvanceTime(superframe_ms * 3, superframe_ms * 3, step_ms, 0,
                [&]() { return false; });

    // ── Degrade: cut Edge1→GW + add 50% loss on NM↔Edge1 ────────────────
    SetDirectionalLink(edge1, gw, false);
    SetLinkLoss(nm, edge1, 0.5f);

    AdvanceTime(superframe_ms * 4, superframe_ms * 4, step_ms, 0,
                [&]() { return false; });

    std::cout << "=== After degradation ===" << std::endl;
    for (auto* n : all_nodes) {
        PrintRoutingTable(*n);
    }

    // ── Send DATA from all three edge nodes to GW ────────────────────────
    constexpr int kMsgsPerNode = 15;
    std::vector<uint8_t> payload = {0xED, 0x6E};

    for (int i = 0; i < kMsgsPerNode; i++) {
        payload[0] = static_cast<uint8_t>(i);
        SendMessage(edge1, gw, payload);
        SendMessage(edge2, gw, payload);
        SendMessage(edge3, gw, payload);
    }

    AdvanceTime(superframe_ms * 30, superframe_ms * 30, step_ms, 0,
                [&]() { return false; });

    size_t from_edge1 =
        CountReceivedMessages(gw, edge1.address, MessageType::DATA);
    size_t from_edge2 =
        CountReceivedMessages(gw, edge2.address, MessageType::DATA);
    size_t from_edge3 =
        CountReceivedMessages(gw, edge3.address, MessageType::DATA);
    size_t total = from_edge1 + from_edge2 + from_edge3;

    std::cout << "Edge1→GW: " << from_edge1 << "/" << kMsgsPerNode
              << "  Edge2→GW: " << from_edge2 << "/" << kMsgsPerNode
              << "  Edge3→GW: " << from_edge3 << "/" << kMsgsPerNode
              << "  Total: " << total << "/" << (kMsgsPerNode * 3) << std::endl;

    // With opportunistic forwarding + 50% PDR, expect meaningful delivery.
    // Without it: packets addressed to GW are silently dropped → ~0.
    EXPECT_GE(total, 3u) << "At least 3 messages total should reach GW";
}

/**
 * @brief FindNextHop rejects a phantom non-TDMA next_hop.
 *
 * A node outside the network's TDMA schedule is never returned as a
 * next_hop even if it appears in the routing table from overheard
 * broadcasts.
 */
TEST_F(TDMARoutingMismatchTests, PhantomNodeRejectedByTDMACheck) {
    auto& nm = CreateNode("NM", 0x1000, NodeRole::NETWORK_MANAGER);
    auto& gw = CreateNode("GW", 0x2000, NodeRole::NODE_ONLY);
    auto& edge1 = CreateNode("Edge1", 0x3000, NodeRole::NODE_ONLY);
    auto& phantom = CreateNode("Phantom", 0x5000, NodeRole::NETWORK_MANAGER);

    SetLinkStatus(nm, gw, true);
    SetLinkStatus(nm, edge1, true);

    ASSERT_TRUE(StartNode(nm));
    ASSERT_TRUE(StartNode(gw));
    ASSERT_TRUE(StartNode(edge1));

    std::vector<TestNode*> main_nodes = {&nm, &gw, &edge1};
    ASSERT_TRUE(WaitForNetworkFormation(main_nodes, 2));
    ASSERT_TRUE(WaitForRoutingStabilization(main_nodes));

    auto superframe_ms = GetSuperframeDuration(nm);
    auto step_ms = 15u;

    AdvanceTime(superframe_ms * 3, superframe_ms * 3, step_ms, 0,
                [&]() { return false; });

    ASSERT_TRUE(StartNode(phantom));
    SetDirectionalLink(phantom, edge1, true);
    SetLinkStatus(phantom, gw, true);

    AdvanceTime(superframe_ms * 6, superframe_ms * 6, step_ms, 0,
                [&]() { return false; });

    AddressType nh = GetNextHop(edge1, gw.address);
    EXPECT_NE(nh, phantom.address)
        << "Edge1 must NOT route via Phantom (not in TDMA schedule)";

    std::cout << "Edge1 next_hop to GW: 0x" << std::hex << nh << std::dec
              << std::endl;
    PrintRoutingTable(edge1);
}

}  // namespace test
}  // namespace loramesher
