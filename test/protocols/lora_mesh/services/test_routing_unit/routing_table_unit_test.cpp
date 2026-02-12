/**
 * @file routing_table_unit_test.cpp
 * @brief Unit tests for DistanceVectorRoutingTable in isolation
 *
 * Tests the routing table logic without the full protocol stack to help
 * isolate routing algorithm issues from integration/timing issues.
 */

#include <gtest/gtest.h>

#include "protocols/lora_mesh/routing/distance_vector_routing_table.hpp"
#include "types/protocols/lora_mesh/network_node_route.hpp"

namespace loramesher {
namespace test {

using protocols::lora_mesh::DistanceVectorRoutingTable;
using types::protocols::lora_mesh::NetworkNodeRoute;

/**
 * @brief Test fixture for routing table unit tests
 */
class RoutingTableUnitTest : public ::testing::Test {
   protected:
    static constexpr AddressType kLocalAddress = 0x1000;
    static constexpr AddressType kNeighbor1 = 0x1001;
    static constexpr AddressType kNeighbor2 = 0x1002;
    static constexpr AddressType kNeighbor3 = 0x1003;
    static constexpr AddressType kRemoteNode = 0x2000;
    static constexpr uint32_t kCurrentTime = 1000000;
    static constexpr uint8_t kMaxHops = 10;
    static constexpr uint8_t kGoodQuality = 200;
    static constexpr uint8_t kMediumQuality = 128;
    static constexpr uint8_t kPoorQuality = 50;

    void SetUp() override {
        routing_table_ =
            std::make_unique<DistanceVectorRoutingTable>(kLocalAddress);
    }

    void TearDown() override { routing_table_.reset(); }

    /**
     * @brief Add a direct neighbor to the routing table
     */
    void AddDirectNeighbor(AddressType neighbor, uint8_t link_quality = 200) {
        routing_table_->UpdateRoute(neighbor,  // source
                                    neighbor,  // destination (same as source
                                               // for direct neighbor)
                                    1,         // hop_count
                                    link_quality, 0, 0, kCurrentTime);
    }

    /**
     * @brief Simulate receiving a routing table message from a neighbor
     */
    void ReceiveRoutingMessage(AddressType source,
                               const std::vector<RoutingTableEntry>& entries,
                               uint8_t local_link_quality = 200) {
        routing_table_->ProcessRoutingTableMessage(
            source, entries, kCurrentTime, local_link_quality, kMaxHops);
    }

    /**
     * @brief Create a routing table entry
     */
    RoutingTableEntry CreateEntry(AddressType dest, uint8_t hop_count,
                                  uint8_t link_quality = 200) {
        RoutingTableEntry entry;
        entry.destination = dest;
        entry.hop_count = hop_count;
        entry.link_quality = link_quality;
        entry.allocated_data_slots = 0;
        entry.capabilities = 0;
        return entry;
    }

    std::unique_ptr<DistanceVectorRoutingTable> routing_table_;
};

// =============================================================================
// Basic Route Operations Tests
// =============================================================================

TEST_F(RoutingTableUnitTest, EmptyTableReturnsNoRoute) {
    EXPECT_EQ(routing_table_->FindNextHop(kNeighbor1), 0);
    EXPECT_EQ(routing_table_->GetSize(), 0);
}

TEST_F(RoutingTableUnitTest, AddDirectNeighbor) {
    AddDirectNeighbor(kNeighbor1);

    EXPECT_EQ(routing_table_->GetSize(), 1);
    EXPECT_TRUE(routing_table_->IsNodePresent(kNeighbor1));
    EXPECT_EQ(routing_table_->FindNextHop(kNeighbor1), kNeighbor1);
}

TEST_F(RoutingTableUnitTest, DirectNeighborHasHopCountOne) {
    AddDirectNeighbor(kNeighbor1);

    const auto& nodes = routing_table_->GetNodes();
    ASSERT_EQ(nodes.size(), 1);
    EXPECT_EQ(nodes[0].routing_entry.hop_count, 1);
    EXPECT_TRUE(nodes[0].IsDirectNeighbor());
}

TEST_F(RoutingTableUnitTest, RemoveNode) {
    AddDirectNeighbor(kNeighbor1);
    EXPECT_TRUE(routing_table_->IsNodePresent(kNeighbor1));

    EXPECT_TRUE(routing_table_->RemoveNode(kNeighbor1));
    EXPECT_FALSE(routing_table_->IsNodePresent(kNeighbor1));
    EXPECT_EQ(routing_table_->GetSize(), 0);
}

TEST_F(RoutingTableUnitTest, ClearRemovesAllNodes) {
    AddDirectNeighbor(kNeighbor1);
    AddDirectNeighbor(kNeighbor2);
    AddDirectNeighbor(kNeighbor3);
    EXPECT_EQ(routing_table_->GetSize(), 3);

    routing_table_->Clear();
    EXPECT_EQ(routing_table_->GetSize(), 0);
}

// =============================================================================
// Route Selection Tests (IsBetterRoute logic)
// =============================================================================

TEST_F(RoutingTableUnitTest, ShorterRouteIsPreferred) {
    // First learn about remote node via Neighbor1 with 3 hops
    std::vector<RoutingTableEntry> entries1;
    entries1.push_back(CreateEntry(kRemoteNode, 2, kGoodQuality));  // Will be 3
                                                                    // hops via
                                                                    // Neighbor1
    ReceiveRoutingMessage(kNeighbor1, entries1);

    // Verify initial route
    EXPECT_EQ(routing_table_->FindNextHop(kRemoteNode), kNeighbor1);
    const auto& nodes1 = routing_table_->GetNodes();
    auto it1 = std::find_if(
        nodes1.begin(), nodes1.end(), [](const NetworkNodeRoute& n) {
            return n.routing_entry.destination == kRemoteNode;
        });
    ASSERT_NE(it1, nodes1.end());
    EXPECT_EQ(it1->routing_entry.hop_count, 3);

    // Now learn about same remote node via Neighbor2 with 2 hops (better!)
    std::vector<RoutingTableEntry> entries2;
    entries2.push_back(CreateEntry(kRemoteNode, 1, kGoodQuality));  // Will be 2
                                                                    // hops via
                                                                    // Neighbor2
    ReceiveRoutingMessage(kNeighbor2, entries2);

    // Verify route updated to shorter path
    EXPECT_EQ(routing_table_->FindNextHop(kRemoteNode), kNeighbor2);
    const auto& nodes2 = routing_table_->GetNodes();
    auto it2 = std::find_if(
        nodes2.begin(), nodes2.end(), [](const NetworkNodeRoute& n) {
            return n.routing_entry.destination == kRemoteNode;
        });
    ASSERT_NE(it2, nodes2.end());
    EXPECT_EQ(it2->routing_entry.hop_count, 2);
}

TEST_F(RoutingTableUnitTest, LongerRouteNotPreferredOverShorter) {
    // First learn about remote node via Neighbor1 with 2 hops
    std::vector<RoutingTableEntry> entries1;
    entries1.push_back(CreateEntry(kRemoteNode, 1, kGoodQuality));  // 2 hops
    ReceiveRoutingMessage(kNeighbor1, entries1);

    // Verify initial route
    EXPECT_EQ(routing_table_->FindNextHop(kRemoteNode), kNeighbor1);

    // Now receive a worse route via Neighbor2 with 4 hops
    std::vector<RoutingTableEntry> entries2;
    entries2.push_back(CreateEntry(kRemoteNode, 3, kGoodQuality));  // 4 hops
    ReceiveRoutingMessage(kNeighbor2, entries2);

    // Route should NOT change - still prefer shorter path
    EXPECT_EQ(routing_table_->FindNextHop(kRemoteNode), kNeighbor1);
}

TEST_F(RoutingTableUnitTest, BetterQualityPreferredWhenSameHops) {
    // This test verifies that when two routes have the same hop count,
    // the one with better quality is preferred.
    //
    // Use UpdateRoute directly to bypass link stats complexity and test
    // the core IsBetterRoute logic with explicit quality values.

    // First add a route via Neighbor1 with poor quality
    bool changed =
        routing_table_->UpdateRoute(kNeighbor1,    // source
                                    kRemoteNode,   // destination
                                    2,             // hop_count
                                    kPoorQuality,  // link_quality = 50
                                    0,             // allocated_data_slots
                                    0,             // capabilities
                                    kCurrentTime);

    EXPECT_TRUE(changed);
    EXPECT_EQ(routing_table_->FindNextHop(kRemoteNode), kNeighbor1);

    // Verify the quality stored
    const auto& nodes_before = routing_table_->GetNodes();
    auto it_before =
        std::find_if(nodes_before.begin(), nodes_before.end(),
                     [](const NetworkNodeRoute& n) {
                         return n.routing_entry.destination == kRemoteNode;
                     });
    ASSERT_NE(it_before, nodes_before.end());

    // Now add a route via Neighbor2 with better quality (same hop count)
    // Cost via Neighbor1: 2*35 + (255-50) = 70 + 205 = 275
    // Cost via Neighbor2: 2*35 + (255-200) = 70 + 55 = 125
    // Neighbor2 route should be preferred (lower cost)
    changed = routing_table_->UpdateRoute(
        kNeighbor2,    // source
        kRemoteNode,   // destination
        2,             // same hop_count
        kGoodQuality,  // link_quality = 200 (better)
        0,             // allocated_data_slots
        0,             // capabilities
        kCurrentTime);

    EXPECT_TRUE(changed);
    EXPECT_EQ(routing_table_->FindNextHop(kRemoteNode), kNeighbor2)
        << "Route should change to higher quality path (same hops)";
}

// =============================================================================
// ProcessRoutingTableMessage Tests
// =============================================================================

TEST_F(RoutingTableUnitTest, ProcessRoutingMessageAddsSourceAsDirectNeighbor) {
    std::vector<RoutingTableEntry> entries;  // Empty - just to trigger
                                             // processing
    ReceiveRoutingMessage(kNeighbor1, entries);

    // Source should be added as direct neighbor
    EXPECT_TRUE(routing_table_->IsNodePresent(kNeighbor1));
    const auto& nodes = routing_table_->GetNodes();
    auto it =
        std::find_if(nodes.begin(), nodes.end(), [](const NetworkNodeRoute& n) {
            return n.routing_entry.destination == kNeighbor1;
        });
    ASSERT_NE(it, nodes.end());
    EXPECT_EQ(it->routing_entry.hop_count, 1);
    EXPECT_EQ(it->next_hop, kNeighbor1);
}

TEST_F(RoutingTableUnitTest, ProcessRoutingMessageAddsRemoteNodes) {
    std::vector<RoutingTableEntry> entries;
    entries.push_back(CreateEntry(kRemoteNode, 1, kGoodQuality));  // 2 hops via
                                                                   // kNeighbor1
    ReceiveRoutingMessage(kNeighbor1, entries);

    // Source should be added as direct neighbor
    EXPECT_TRUE(routing_table_->IsNodePresent(kNeighbor1));

    // Remote node should be added with hop count + 1
    EXPECT_TRUE(routing_table_->IsNodePresent(kRemoteNode));
    const auto& nodes = routing_table_->GetNodes();
    auto it =
        std::find_if(nodes.begin(), nodes.end(), [](const NetworkNodeRoute& n) {
            return n.routing_entry.destination == kRemoteNode;
        });
    ASSERT_NE(it, nodes.end());
    EXPECT_EQ(it->routing_entry.hop_count, 2);  // Entry's 1 hop + 1 = 2
    EXPECT_EQ(it->next_hop, kNeighbor1);
}

TEST_F(RoutingTableUnitTest, ProcessRoutingMessageSkipsSelf) {
    std::vector<RoutingTableEntry> entries;
    entries.push_back(CreateEntry(kLocalAddress, 1, kGoodQuality));  // Entry
                                                                     // for
                                                                     // local
                                                                     // node
    entries.push_back(CreateEntry(kRemoteNode, 1, kGoodQuality));
    ReceiveRoutingMessage(kNeighbor1, entries);

    // Local address should NOT be in routing table
    EXPECT_FALSE(routing_table_->IsNodePresent(kLocalAddress));
    // But remote node should be added
    EXPECT_TRUE(routing_table_->IsNodePresent(kRemoteNode));
}

TEST_F(RoutingTableUnitTest, ProcessRoutingMessageRespectsMaxHops) {
    // Create a custom routing table with low max hops for testing
    auto table = DistanceVectorRoutingTable(kLocalAddress);

    std::vector<RoutingTableEntry> entries;
    entries.push_back(CreateEntry(kRemoteNode, 9, kGoodQuality));  // 10 hops
                                                                   // total
                                                                   // (exceeds
                                                                   // limit
                                                                   // usually)

    // Process with max_hops = 5 (so 9+1=10 should be rejected)
    table.ProcessRoutingTableMessage(kNeighbor1, entries, kCurrentTime,
                                     kGoodQuality, 5);  // max_hops = 5

    // Remote node should NOT be added (exceeds max hops)
    EXPECT_FALSE(table.IsNodePresent(kRemoteNode));
    // But source should still be added as direct neighbor
    EXPECT_TRUE(table.IsNodePresent(kNeighbor1));
}

// =============================================================================
// Full Mesh Topology Tests (simulating the failing integration test)
// =============================================================================

TEST_F(RoutingTableUnitTest, FullMeshDirectNeighborsAllHaveHopCountOne) {
    // Simulate a 4-node full mesh from Node1's perspective
    // All nodes should be direct neighbors with hop_count = 1

    // Receive routing messages from all 3 neighbors
    // Each neighbor has empty entries (they just discovered us)
    std::vector<RoutingTableEntry> empty_entries;
    ReceiveRoutingMessage(kNeighbor1, empty_entries);
    ReceiveRoutingMessage(kNeighbor2, empty_entries);
    ReceiveRoutingMessage(kNeighbor3, empty_entries);

    // All neighbors should have hop_count = 1
    const auto& nodes = routing_table_->GetNodes();
    EXPECT_EQ(nodes.size(), 3);

    for (const auto& node : nodes) {
        EXPECT_EQ(node.routing_entry.hop_count, 1)
            << "Node 0x" << std::hex << node.routing_entry.destination
            << " should have hop_count=1 but has "
            << static_cast<int>(node.routing_entry.hop_count);
        EXPECT_EQ(node.next_hop, node.routing_entry.destination)
            << "Direct neighbor's next_hop should be itself";
    }
}

TEST_F(RoutingTableUnitTest, FullMeshRoutingConvergence) {
    // Simulate full mesh network formation from Node1 (0x1000) perspective
    // Topology: Node1(0x1000) - Node2(0x1001) - Node3(0x1002) - Node4(0x1003)
    // All nodes directly connected to all others

    // Step 1: First, receive direct neighbor announcements (empty routing
    // tables)
    std::vector<RoutingTableEntry> empty;
    ReceiveRoutingMessage(kNeighbor1, empty);  // Node2 announces itself
    ReceiveRoutingMessage(kNeighbor2, empty);  // Node3 announces itself
    ReceiveRoutingMessage(kNeighbor3, empty);  // Node4 announces itself

    // At this point, all neighbors should be direct (hop=1)
    EXPECT_EQ(routing_table_->GetSize(), 3);

    // Step 2: Now simulate the next round where neighbors advertise their
    // routes In a full mesh, Node2 knows about Node3 and Node4 directly (1
    // hop) So Node2 advertises: Node3(1 hop), Node4(1 hop)
    std::vector<RoutingTableEntry> node2_routes;
    node2_routes.push_back(CreateEntry(kNeighbor2, 1, kGoodQuality));  // Node3,
                                                                       // 1 hop
    node2_routes.push_back(CreateEntry(kNeighbor3, 1, kGoodQuality));  // Node4,
                                                                       // 1 hop
    ReceiveRoutingMessage(kNeighbor1, node2_routes);

    // Node3 and Node4 should STILL be direct neighbors (1 hop) because we
    // already have better routes to them
    const auto& nodes = routing_table_->GetNodes();

    for (const auto& node : nodes) {
        EXPECT_EQ(node.routing_entry.hop_count, 1)
            << "In full mesh, all nodes should be 1 hop away, but node 0x"
            << std::hex << node.routing_entry.destination << " has "
            << static_cast<int>(node.routing_entry.hop_count) << " hops";
    }
}

TEST_F(RoutingTableUnitTest, DirectNeighborShouldOverrideIndirectRoute) {
    // This is the key test: If we learn about a node indirectly first,
    // then receive a direct routing message from them, the hop count
    // should update to 1.

    // Step 1: Learn about Node3 (0x1002) indirectly via Node2 (0x1001)
    // Node2 tells us: "I can reach Node3 in 1 hop"
    // So we think: Node3 is 2 hops away via Node2
    std::vector<RoutingTableEntry> node2_routes;
    node2_routes.push_back(CreateEntry(kNeighbor2, 1, kGoodQuality));
    ReceiveRoutingMessage(kNeighbor1, node2_routes);

    // Verify Node3 is currently 2 hops away
    const auto& nodes_before = routing_table_->GetNodes();
    auto it_before =
        std::find_if(nodes_before.begin(), nodes_before.end(),
                     [](const NetworkNodeRoute& n) {
                         return n.routing_entry.destination == kNeighbor2;
                     });
    ASSERT_NE(it_before, nodes_before.end());
    EXPECT_EQ(it_before->routing_entry.hop_count, 2)
        << "Node3 should be 2 hops via Node2 initially";
    EXPECT_EQ(it_before->next_hop, kNeighbor1);

    // Step 2: Now receive a direct routing message from Node3
    // This should update our route to Node3 to 1 hop
    std::vector<RoutingTableEntry> empty;
    ReceiveRoutingMessage(kNeighbor2, empty);  // Node3 directly contacts us

    // Verify Node3 is now 1 hop away (direct neighbor)
    const auto& nodes_after = routing_table_->GetNodes();
    auto it_after = std::find_if(
        nodes_after.begin(), nodes_after.end(), [](const NetworkNodeRoute& n) {
            return n.routing_entry.destination == kNeighbor2;
        });
    ASSERT_NE(it_after, nodes_after.end());
    EXPECT_EQ(it_after->routing_entry.hop_count, 1)
        << "Node3 should be 1 hop after direct contact";
    EXPECT_EQ(it_after->next_hop, kNeighbor2)
        << "Next hop should be Node3 itself (direct neighbor)";
}

// =============================================================================
// Route Cost Calculation Tests
// =============================================================================

TEST_F(RoutingTableUnitTest, RouteCostCalculation) {
    // Verify the cost formula: cost = hop_count * 35 + (255 - link_quality)

    // 1 hop, quality 255 -> cost = 1*35 + (255-255) = 35
    EXPECT_EQ(NetworkNodeRoute::CalculateRouteCost(1, 255), 35);

    // 1 hop, quality 200 -> cost = 1*35 + (255-200) = 35 + 55 = 90
    EXPECT_EQ(NetworkNodeRoute::CalculateRouteCost(1, 200), 90);

    // 2 hops, quality 255 -> cost = 2*35 + 0 = 70
    EXPECT_EQ(NetworkNodeRoute::CalculateRouteCost(2, 255), 70);

    // 2 hops, quality 200 -> cost = 70 + 55 = 125
    EXPECT_EQ(NetworkNodeRoute::CalculateRouteCost(2, 200), 125);

    // 1 hop quality 200 (cost 90) is better than 2 hops quality 255 (cost 70)?
    // No - 2 hops with 255 quality (cost 70) is actually better than 1 hop with
    // 200 quality (cost 90)

    // Key insight: The cost formula allows trading hop count for quality
    // Each hop costs 35 points, so a quality difference of 35 justifies one
    // extra hop
}

TEST_F(RoutingTableUnitTest, IsBetterRouteLogic) {
    // Test that IsBetterRouteThan() correctly compares routes

    // Route A: 1 hop, quality 200
    NetworkNodeRoute routeA(kRemoteNode, kNeighbor1, 1, 200, kCurrentTime);
    routeA.is_active = true;

    // Route B: 2 hops, quality 255 (cost: 70 vs 90 - B is cheaper!)
    NetworkNodeRoute routeB(kRemoteNode, kNeighbor2, 2, 255, kCurrentTime);
    routeB.is_active = true;

    // According to cost formula, B (cost 70) is better than A (cost 90)
    EXPECT_TRUE(routeB.IsBetterRouteThan(routeA))
        << "2 hops with perfect quality should beat 1 hop with 200 quality";

    // Route C: 1 hop, quality 255 (cost: 35 - best)
    NetworkNodeRoute routeC(kRemoteNode, kNeighbor3, 1, 255, kCurrentTime);
    routeC.is_active = true;

    EXPECT_TRUE(routeC.IsBetterRouteThan(routeA));
    EXPECT_TRUE(routeC.IsBetterRouteThan(routeB));

    // Route D: 3 hops, quality 255 (cost: 105)
    NetworkNodeRoute routeD(kRemoteNode, kNeighbor1, 3, 255, kCurrentTime);
    routeD.is_active = true;

    EXPECT_FALSE(routeD.IsBetterRouteThan(routeA))
        << "3 hops should not beat 1 hop with 200 quality";
}

// =============================================================================
// Link Failure Scenario Tests
// =============================================================================

TEST_F(RoutingTableUnitTest, RouteUpdateAfterLinkFailureScenario) {
    // Simulate the failing integration test scenario:
    // 4-node full mesh, then break link between Node1 and Node3

    // Step 1: Set up full mesh - all nodes are direct neighbors
    std::vector<RoutingTableEntry> empty;
    ReceiveRoutingMessage(kNeighbor1, empty);  // Node2
    ReceiveRoutingMessage(kNeighbor2, empty);  // Node3
    ReceiveRoutingMessage(kNeighbor3, empty);  // Node4

    // Verify all are 1 hop
    for (AddressType addr : {kNeighbor1, kNeighbor2, kNeighbor3}) {
        const auto& nodes = routing_table_->GetNodes();
        auto it = std::find_if(nodes.begin(), nodes.end(),
                               [addr](const NetworkNodeRoute& n) {
                                   return n.routing_entry.destination == addr;
                               });
        ASSERT_NE(it, nodes.end());
        EXPECT_EQ(it->routing_entry.hop_count, 1);
    }

    // Step 2: Simulate link failure to Node3 (0x1002)
    // We stop receiving direct messages from Node3, but Node2 still knows about
    // Node3

    // Remove the direct route to Node3
    routing_table_->RemoveNode(kNeighbor2);

    // Step 3: Node2 advertises that it can still reach Node3 (1 hop from Node2
    // = 2 hops from us)
    std::vector<RoutingTableEntry> node2_routes;
    node2_routes.push_back(CreateEntry(kNeighbor2, 1, kGoodQuality));  // Node3
                                                                       // via
                                                                       // Node2
    ReceiveRoutingMessage(kNeighbor1, node2_routes);

    // Verify Node3 is now reachable via Node2 with 2 hops
    const auto& nodes = routing_table_->GetNodes();
    auto it =
        std::find_if(nodes.begin(), nodes.end(), [](const NetworkNodeRoute& n) {
            return n.routing_entry.destination == kNeighbor2;
        });
    ASSERT_NE(it, nodes.end());
    EXPECT_EQ(it->routing_entry.hop_count, 2)
        << "After link failure, route should be 2 hops via Node2";
    EXPECT_EQ(it->next_hop, kNeighbor1) << "Next hop should be Node2";
}

// =============================================================================
// Line Topology Tests
// =============================================================================

TEST_F(RoutingTableUnitTest, LineTopologyRouting) {
    // Test line topology: Node1 - Node2 - Node3
    // Node1 (local) can only reach Node2 directly
    // Node1 reaches Node3 via Node2 (2 hops)

    // Step 1: Node2 announces itself (direct neighbor)
    std::vector<RoutingTableEntry> empty;
    ReceiveRoutingMessage(kNeighbor1, empty);

    EXPECT_TRUE(routing_table_->IsNodePresent(kNeighbor1));
    EXPECT_EQ(routing_table_->FindNextHop(kNeighbor1), kNeighbor1);

    // Step 2: Node2 advertises that it knows about Node3 (1 hop from Node2)
    std::vector<RoutingTableEntry> node2_routes;
    node2_routes.push_back(CreateEntry(kNeighbor2, 1, kGoodQuality));  // Node3,
                                                                       // 1 hop
                                                                       // from
                                                                       // Node2
    ReceiveRoutingMessage(kNeighbor1, node2_routes);

    // Verify Node3 is reachable via Node2 with 2 hops
    EXPECT_TRUE(routing_table_->IsNodePresent(kNeighbor2));
    EXPECT_EQ(routing_table_->FindNextHop(kNeighbor2), kNeighbor1);

    const auto& nodes = routing_table_->GetNodes();
    auto it =
        std::find_if(nodes.begin(), nodes.end(), [](const NetworkNodeRoute& n) {
            return n.routing_entry.destination == kNeighbor2;
        });
    ASSERT_NE(it, nodes.end());
    EXPECT_EQ(it->routing_entry.hop_count, 2);
}

// =============================================================================
// Additional Edge Case Tests
// =============================================================================

TEST_F(RoutingTableUnitTest, UpdateRouteViaUpdateRouteMethod) {
    // Test the UpdateRoute method directly (used by other parts of the system)

    // Add a route with 2 hops
    bool changed = routing_table_->UpdateRoute(kNeighbor1,    // source
                                               kRemoteNode,   // destination
                                               2,             // hop_count
                                               kGoodQuality,  // link_quality
                                               0,  // allocated_data_slots
                                               0,  // capabilities
                                               kCurrentTime  // current_time
    );

    EXPECT_TRUE(changed);
    EXPECT_TRUE(routing_table_->IsNodePresent(kRemoteNode));
    EXPECT_EQ(routing_table_->FindNextHop(kRemoteNode), kNeighbor1);

    // Update to a better route (1 hop)
    changed = routing_table_->UpdateRoute(kNeighbor2,    // different source
                                          kRemoteNode,   // same destination
                                          1,             // shorter hop_count
                                          kGoodQuality,  // link_quality
                                          0,             // allocated_data_slots
                                          0,             // capabilities
                                          kCurrentTime   // current_time
    );

    EXPECT_TRUE(changed);
    EXPECT_EQ(routing_table_->FindNextHop(kRemoteNode), kNeighbor2);
}

TEST_F(RoutingTableUnitTest, GetRoutingEntriesExcludesAddress) {
    // Add several neighbors
    std::vector<RoutingTableEntry> empty;
    ReceiveRoutingMessage(kNeighbor1, empty);
    ReceiveRoutingMessage(kNeighbor2, empty);
    ReceiveRoutingMessage(kNeighbor3, empty);

    // Get entries excluding kNeighbor2
    auto entries = routing_table_->GetRoutingEntries(kNeighbor2);

    // Should have 2 entries (kNeighbor1 and kNeighbor3)
    EXPECT_EQ(entries.size(), 2);

    // kNeighbor2 should NOT be in the list
    for (const auto& entry : entries) {
        EXPECT_NE(entry.destination, kNeighbor2);
    }
}

TEST_F(RoutingTableUnitTest, MaxNodesLimit) {
    // Create a routing table with max 3 nodes
    auto limited_table = DistanceVectorRoutingTable(kLocalAddress, 3);

    // Add 3 neighbors
    std::vector<RoutingTableEntry> empty;
    limited_table.ProcessRoutingTableMessage(kNeighbor1, empty, kCurrentTime,
                                             kGoodQuality, kMaxHops);
    limited_table.ProcessRoutingTableMessage(
        kNeighbor2, empty, kCurrentTime + 100, kGoodQuality, kMaxHops);
    limited_table.ProcessRoutingTableMessage(
        kNeighbor3, empty, kCurrentTime + 200, kGoodQuality, kMaxHops);

    EXPECT_EQ(limited_table.GetSize(), 3);

    // Try to add a 4th node - should replace oldest
    constexpr AddressType kNeighbor4 = 0x1004;
    limited_table.ProcessRoutingTableMessage(
        kNeighbor4, empty, kCurrentTime + 300, kGoodQuality, kMaxHops);

    // Should still have 3 nodes
    EXPECT_EQ(limited_table.GetSize(), 3);

    // The oldest node (kNeighbor1) should have been removed
    EXPECT_FALSE(limited_table.IsNodePresent(kNeighbor1));
    EXPECT_TRUE(limited_table.IsNodePresent(kNeighbor4));
}

TEST_F(RoutingTableUnitTest, InactiveNodesRemovedOnTimeout) {
    // Add some neighbors
    std::vector<RoutingTableEntry> empty;
    ReceiveRoutingMessage(kNeighbor1, empty);
    ReceiveRoutingMessage(kNeighbor2, empty);

    EXPECT_EQ(routing_table_->GetSize(), 2);

    // Remove inactive nodes with a timeout that has passed
    uint32_t future_time = kCurrentTime + 100000;  // Way in the future
    size_t removed =
        routing_table_->RemoveInactiveNodes(future_time, 1000, 1000);

    // Both nodes should be removed (they're "old" relative to future_time)
    EXPECT_EQ(removed, 2);
    EXPECT_EQ(routing_table_->GetSize(), 0);
}

TEST_F(RoutingTableUnitTest, SelfAddressSkippedInRouteMessages) {
    // Routing messages should skip entries for the local address
    std::vector<RoutingTableEntry> entries;
    entries.push_back(
        CreateEntry(kLocalAddress, 1, kGoodQuality));  // Should be skipped
    entries.push_back(
        CreateEntry(kRemoteNode, 1, kGoodQuality));  // Should be added

    ReceiveRoutingMessage(kNeighbor1, entries);

    // Local address should NOT be in routing table
    EXPECT_FALSE(routing_table_->IsNodePresent(kLocalAddress));
    // Remote node should be added
    EXPECT_TRUE(routing_table_->IsNodePresent(kRemoteNode));
}

TEST_F(RoutingTableUnitTest, FindNextHopForSelfReturnsLocalAddress) {
    // Finding next hop to ourselves should return local address
    EXPECT_EQ(routing_table_->FindNextHop(kLocalAddress), kLocalAddress);
}

}  // namespace test
}  // namespace loramesher
