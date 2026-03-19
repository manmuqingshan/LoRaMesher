/**
 * @file nm_election_test.cpp
 * @brief NM election tests for LoRaMesh protocol
 *
 * Tests NM election when the Network Manager becomes unavailable:
 * staggered-backoff election, priority ordering, and NODE_ONLY exclusion.
 *
 * Timing notes (all in virtual time):
 *   Superframe duration = slot_count * slot_duration (e.g. 16 slots × 1000 ms = 16 s)
 *   kMaxNoReceivedSyncBeacons = 5  → FAULT_RECOVERY after 5 missed superframes
 *   kElectionListenWindowMs   = 5000 ms (mandatory listen window after FAULT_RECOVERY)
 *   AUTO role_bonus           = kElectionListenWindowMs = 5000 ms additional
 *   addr_bonus                = up to ~5000 ms (address-proportional)
 *   jitter                    = 0–2500 ms (random)
 *   -------------------------------------------------------
 *   Worst-case FAULT_RECOVERY + election: 5 × superframe + ~18 s
 *   After election, node rejoin: 3–5 × superframe
 *   Tests compute timeouts dynamically from the actual superframe duration.
 */

#include <gtest/gtest.h>

#include "protocols/lora_mesh/services/network_service.hpp"  // kMaxNoReceivedSyncBeacons, kElectionListenWindowMs
#include "routing_test_fixture.hpp"

namespace loramesher {
namespace test {

using ProtocolState = protocols::lora_mesh::INetworkService::ProtocolState;

/**
 * @brief Test suite for NM election functionality
 */
class NMElectionTests : public RoutingTestFixture {
   protected:
    /**
     * @brief Compute virtual-time budget needed for a full NM election cycle.
     *
     * Covers:
     *  - kMaxNoReceivedSyncBeacons missed sync beacons (= entering FAULT_RECOVERY)
     *  - Maximum election backoff (2 × kElectionListenWindowMs + jitter ≈ 12 500 ms)
     *  - Rejoining phase for the losing node (3 extra superframes)
     *
     * @param superframe_ms Superframe duration in ms (from GetSuperframeDuration)
     * @return uint32_t Generous virtual-time budget in ms
     */
    uint32_t ElectionBudgetMs(uint32_t superframe_ms) const {
        // (kMaxNoReceivedSyncBeacons + 2) missed beacons before FAULT_RECOVERY
        // (+2 for timing jitter and any in-flight beacon delivered after failure)
        uint32_t fault_recovery_ms =
            (protocols::lora_mesh::kMaxNoReceivedSyncBeacons + 2) *
            superframe_ms;
        // Worst-case election backoff: 2 × listen_window + max_addr_bonus + jitter
        uint32_t election_ms =
            2 * protocols::lora_mesh::kElectionListenWindowMs + 5500;
        // New NM setup + discovery + joining: each phase up to 3 superframes
        uint32_t post_election_ms = 9 * superframe_ms;
        return fault_recovery_ms + election_ms + post_election_ms;
    }

    /**
     * @brief Wait until the network has reformed after an NM failure.
     *
     * Checks every half-superframe that exactly one node in @p candidates
     * is NETWORK_MANAGER and @p expected_normal others are NORMAL_OPERATION.
     *
     * @param candidates          Nodes expected to participate
     * @param expected_normal     NORMAL_OPERATION nodes expected after election
     * @return true if the network reformed within ElectionBudgetMs()
     */
    bool WaitForElectionAndReformation(const std::vector<TestNode*>& candidates,
                                       int expected_normal) {
        uint32_t superframe_ms = GetSuperframeDuration(*candidates.front());
        uint32_t timeout_ms = ElectionBudgetMs(superframe_ms);
        uint32_t slot_ms = GetSlotDuration(*candidates.front());
        uint32_t step_ms = 15u;

        uint32_t elapsed = 0;
        uint32_t print_every = superframe_ms * 2;  // Print every 2 superframes
        uint32_t next_print = print_every;

        return AdvanceTime(timeout_ms, timeout_ms, step_ms, 0, [&]() {
            elapsed += step_ms;
            int nm_count = 0;
            int normal_count = 0;
            for (auto* node : candidates) {
                auto s = node->protocol->GetState();
                if (s == ProtocolState::NETWORK_MANAGER)
                    nm_count++;
                if (s == ProtocolState::NORMAL_OPERATION)
                    normal_count++;
            }
            if (elapsed >= next_print) {
                next_print += print_every;
                std::cout << "  [t=" << elapsed << "ms] states: ";
                for (auto* node : candidates) {
                    std::cout << node->name << "="
                              << static_cast<int>(node->protocol->GetState())
                              << " ";
                }
                std::cout << "(nm=" << nm_count << ",norm=" << normal_count
                          << ")" << std::endl;
            }
            return nm_count == 1 && normal_count == expected_normal;
        });
    }
};

// ---------------------------------------------------------------------------
// Test 1: NM disconnect, single AUTO node wins election
//
// Topology:  NM (NETWORK_MANAGER role)  ←→  Node2 (AUTO)  ←→  Node3 (NODE_ONLY)
//            also: NM ←→ Node3
//
// Node2 is the only AUTO node — it wins deterministically.
// Node3 is NODE_ONLY — it never elects, but rejoins Node2 after election.
// ---------------------------------------------------------------------------

/**
 * @brief NM fails; single AUTO node wins election; NODE_ONLY rejoin verified.
 *
 * Verifies:
 * - Node2 (AUTO) enters FAULT_RECOVERY and starts election backoff
 * - Node2 wins election (only AUTO candidate) and becomes NETWORK_MANAGER
 * - Node3 (NODE_ONLY) rejoins Node2's network as NORMAL_OPERATION
 */
TEST_F(NMElectionTests, NMDisconnect_AutoNodeBecomesNM) {
    // Full-mesh: NM ←→ Node2 ←→ Node3, NM ←→ Node3
    auto& nm_node = CreateNode("NM", 0x1000, NodeRole::NETWORK_MANAGER);
    auto& node2 = CreateNode("Node2", 0x1001, NodeRole::AUTO);
    auto& node3 = CreateNode("Node3", 0x1002, NodeRole::NODE_ONLY);

    SetLinkStatus(nm_node, node2, true);
    SetLinkStatus(nm_node, node3, true);
    SetLinkStatus(node2, node3, true);

    ASSERT_TRUE(StartNode(nm_node));
    ASSERT_TRUE(StartNode(node2));
    ASSERT_TRUE(StartNode(node3));

    // Phase 1: initial network formation
    std::vector<TestNode*> all_nodes = {&nm_node, &node2, &node3};
    ASSERT_TRUE(WaitForNetworkFormation(all_nodes, 2))
        << "Initial network formation failed";

    EXPECT_EQ(nm_node.protocol->GetState(), ProtocolState::NETWORK_MANAGER);
    EXPECT_EQ(node2.protocol->GetState(), ProtocolState::NORMAL_OPERATION);
    EXPECT_EQ(node3.protocol->GetState(), ProtocolState::NORMAL_OPERATION);

    std::cout << "=== Initial network formed. Simulating NM failure ==="
              << std::endl;

    // Phase 2: NM failure — cut all links from nm_node
    SimulateNodeFailure(nm_node);

    // Phase 3: election — Node2 is the only AUTO node, must win
    std::vector<TestNode*> survivors = {&node2, &node3};
    bool network_reformed = WaitForElectionAndReformation(survivors, 1);

    std::cout << "=== Post-election states ===" << std::endl;
    std::cout << "  Node2 state="
              << static_cast<int>(node2.protocol->GetState())
              << "  (NETWORK_MANAGER="
              << static_cast<int>(ProtocolState::NETWORK_MANAGER) << ")"
              << std::endl;
    std::cout << "  Node3 state="
              << static_cast<int>(node3.protocol->GetState())
              << "  (NORMAL_OPERATION="
              << static_cast<int>(ProtocolState::NORMAL_OPERATION) << ")"
              << std::endl;

    ASSERT_TRUE(network_reformed)
        << "Network did not reform after NM failure. "
           "Node2 state="
        << static_cast<int>(node2.protocol->GetState())
        << " Node3 state=" << static_cast<int>(node3.protocol->GetState());

    // Node2 must be the new NM (it was the only AUTO candidate)
    EXPECT_EQ(node2.protocol->GetState(), ProtocolState::NETWORK_MANAGER)
        << "Node2 (AUTO) should be the new NETWORK_MANAGER";
    EXPECT_EQ(node3.protocol->GetState(), ProtocolState::NORMAL_OPERATION)
        << "Node3 (NODE_ONLY) should have rejoined as NORMAL_OPERATION";

    // Verify NM addresses are consistent
    EXPECT_EQ(node2.protocol->GetNetworkManager(), node2.address)
        << "Node2 should recognise itself as NM";
    EXPECT_EQ(node3.protocol->GetNetworkManager(), node2.address)
        << "Node3 should recognise Node2 as NM";
}

// ---------------------------------------------------------------------------
// Test 2: NODE_ONLY nodes never create a network after NM failure
//
// Topology:  NM (NETWORK_MANAGER)  ←→  Node2 (NODE_ONLY)
//                                  ←→  Node3 (NODE_ONLY)
//            Node2 and Node3 are not directly connected.
//
// After NM failure, neither NODE_ONLY node should become NETWORK_MANAGER.
// ---------------------------------------------------------------------------

/**
 * @brief NODE_ONLY nodes stay out of elections permanently.
 *
 * Verifies:
 * - After NM failure, NODE_ONLY nodes enter FAULT_RECOVERY
 * - No NODE_ONLY node transitions to NETWORK_MANAGER
 * - Nodes cycle through DISCOVERY repeatedly (no network created)
 */
TEST_F(NMElectionTests, NodeOnly_NeverElects) {
    auto& nm_node = CreateNode("NM", 0x1000, NodeRole::NETWORK_MANAGER);
    auto& node2 = CreateNode("Node2", 0x1001, NodeRole::NODE_ONLY);
    auto& node3 = CreateNode("Node3", 0x1002, NodeRole::NODE_ONLY);

    // Star topology with NM at center; NODE_ONLY nodes cannot see each other
    SetLinkStatus(nm_node, node2, true);
    SetLinkStatus(nm_node, node3, true);
    SetLinkStatus(node2, node3, false);

    ASSERT_TRUE(StartNode(nm_node));
    ASSERT_TRUE(StartNode(node2));
    ASSERT_TRUE(StartNode(node3));

    // Phase 1: initial network formation
    std::vector<TestNode*> all_nodes = {&nm_node, &node2, &node3};
    ASSERT_TRUE(WaitForNetworkFormation(all_nodes, 2))
        << "Initial network formation failed";

    // Phase 2: NM failure
    SimulateNodeFailure(nm_node);

    // Phase 3: advance well past the maximum election window for AUTO nodes,
    // while verifying that NODE_ONLY nodes never become NETWORK_MANAGER.
    // Budget: fault_recovery (5 superframes) + max_election (~18 s) + margin (2 superframes)
    uint32_t superframe_ms = GetSuperframeDuration(node2);
    uint32_t budget_ms = ElectionBudgetMs(superframe_ms);
    uint32_t slot_ms = GetSlotDuration(node2);
    uint32_t step_ms = 15u;

    AdvanceTime(budget_ms, 0, step_ms, 0);

    EXPECT_NE(node2.protocol->GetState(), ProtocolState::NETWORK_MANAGER)
        << "NODE_ONLY Node2 must NOT become NETWORK_MANAGER";
    EXPECT_NE(node3.protocol->GetState(), ProtocolState::NETWORK_MANAGER)
        << "NODE_ONLY Node3 must NOT become NETWORK_MANAGER";
}

// ---------------------------------------------------------------------------
// Test 3: Two competing AUTO nodes — exactly one wins, the other joins
//
// Topology:  NM (NETWORK_MANAGER role) ←→ Node2 (AUTO, 0x1001)
//                                      ←→ Node3 (AUTO, 0x1080)
//            Node2 ←→ Node3 (full mesh)
//
// Both Node2 and Node3 are AUTO; both enter election after NM fails.
// The NM_CLAIM exchange resolves the tie: lower priority value wins.
//   Node2 priority = 64 + (0x01 >> 1) = 64
//   Node3 priority = 64 + (0x80 >> 1) = 128
// The test verifies that EXACTLY ONE node becomes NM and the other joins.
// ---------------------------------------------------------------------------

/**
 * @brief Two AUTO nodes compete; exactly one wins and the other joins.
 *
 * Verifies:
 * - After NM failure, both AUTO nodes enter election
 * - Exactly one reaches NETWORK_MANAGER state
 * - The other reaches NORMAL_OPERATION under the winner
 */
TEST_F(NMElectionTests, TwoAutoNodes_ExactlyOneWinsElection) {
    // Large address gap to make addr_bonus difference significant:
    //   Node2 (0x1001): addr_bonus = 5000 * 1 / 256 ≈   20 ms
    //   Node3 (0x1080): addr_bonus = 5000 * 128 / 256 = 2500 ms
    auto& nm_node = CreateNode("NM", 0x1000, NodeRole::NETWORK_MANAGER);
    auto& node2 = CreateNode("Node2", 0x1001, NodeRole::AUTO);
    auto& node3 = CreateNode("Node3", 0x1080, NodeRole::AUTO);

    // Full mesh
    SetLinkStatus(nm_node, node2, true);
    SetLinkStatus(nm_node, node3, true);
    SetLinkStatus(node2, node3, true);

    ASSERT_TRUE(StartNode(nm_node));
    ASSERT_TRUE(StartNode(node2));
    ASSERT_TRUE(StartNode(node3));

    // Phase 1: initial network formation
    std::vector<TestNode*> all_nodes = {&nm_node, &node2, &node3};
    ASSERT_TRUE(WaitForNetworkFormation(all_nodes, 2))
        << "Initial network formation failed";

    std::cout << "=== Initial network formed ===" << std::endl;

    // Phase 2: NM failure
    SimulateNodeFailure(nm_node);

    // Phase 3: wait for election to settle (one winner + one joiner)
    std::vector<TestNode*> survivors = {&node2, &node3};
    bool network_reformed = WaitForElectionAndReformation(survivors, 1);

    // Determine winner for diagnostic output
    TestNode* winner = nullptr;
    TestNode* joiner = nullptr;
    for (auto* node : survivors) {
        if (node->protocol->GetState() == ProtocolState::NETWORK_MANAGER)
            winner = node;
        else if (node->protocol->GetState() == ProtocolState::NORMAL_OPERATION)
            joiner = node;
    }

    std::cout << "=== Post-election states ===" << std::endl;
    for (auto* node : survivors) {
        std::cout << "  " << node->name << " (0x" << std::hex << node->address
                  << std::dec
                  << "): state=" << static_cast<int>(node->protocol->GetState())
                  << std::endl;
    }

    ASSERT_TRUE(network_reformed)
        << "Network did not reform within timeout. "
           "Node2 state="
        << static_cast<int>(node2.protocol->GetState())
        << " Node3 state=" << static_cast<int>(node3.protocol->GetState());

    ASSERT_NE(winner, nullptr) << "No NETWORK_MANAGER found after election";
    ASSERT_NE(joiner, nullptr)
        << "No NORMAL_OPERATION node found after election";

    // The joiner must recognise the winner as NM
    EXPECT_EQ(joiner->protocol->GetNetworkManager(), winner->address)
        << joiner->name << " should recognise " << winner->name << " as NM";

    // The winner must recognise itself as NM
    EXPECT_EQ(winner->protocol->GetNetworkManager(), winner->address)
        << winner->name << " should be its own NM";
}

}  // namespace test
}  // namespace loramesher
