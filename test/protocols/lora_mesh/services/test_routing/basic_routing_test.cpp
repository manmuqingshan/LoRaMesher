/**
 * @file basic_routing_test.cpp
 * @brief Basic routing tests for LoRaMesh protocol
 *
 * Tests fundamental routing functionality with simple 2-3 node topologies.
 */

#include <gtest/gtest.h>

#include <algorithm>

#include "routing_test_fixture.hpp"

namespace loramesher {
namespace test {

/**
 * @brief Test suite for basic routing functionality
 */
class BasicRoutingTests : public RoutingTestFixture {};

/**
 * @brief Test direct neighbor routing (2 nodes)
 *
 * Topology: N1 <-> N2
 *
 * Verifies:
 * - Direct 1-hop route is established
 * - Message delivery works between adjacent nodes
 */
TEST_F(BasicRoutingTests, DirectNeighborRouting) {
    // Create two nodes with explicit roles for deterministic network formation
    auto& node1 = CreateManagerNode("Node1", 0x1001);
    auto& node2 = CreateJoiningNode("Node2", 0x1002);

    // Connect them
    SetLinkStatus(node1, node2, true);

    // Start nodes
    ASSERT_TRUE(StartNode(node1));
    ASSERT_TRUE(StartNode(node2));

    // Wait for network formation (1 normal node + 1 network manager)
    std::vector<TestNode*> nodes = {&node1, &node2};
    ASSERT_TRUE(WaitForNetworkFormation(nodes, 1))
        << "Network formation failed";

    // Wait for routing tables to stabilize
    ASSERT_TRUE(WaitForRoutingStabilization(nodes))
        << "Routing stabilization failed";

    // Verify routing tables
    // Node1 should have 1-hop route to Node2
    EXPECT_TRUE(HasRouteTo(node1, node2.address))
        << "Node1 should have route to Node2";
    EXPECT_EQ(GetHopCount(node1, node2.address), 1)
        << "Route from Node1 to Node2 should be 1 hop";

    // Node2 should have 1-hop route to Node1
    EXPECT_TRUE(HasRouteTo(node2, node1.address))
        << "Node2 should have route to Node1";
    EXPECT_EQ(GetHopCount(node2, node1.address), 1)
        << "Route from Node2 to Node1 should be 1 hop";

    // Send a message from node1 to node2
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
    ASSERT_TRUE(SendMessage(node1, node2, payload)) << "Failed to send message";

    auto superframe_duration = GetSuperframeDuration(*nodes[0]);
    uint32_t step_ms = 15u;

    // Wait for message to be received
    bool received =
        AdvanceTime(5000, superframe_duration * 3, step_ms, 0, [&]() {
            return HasReceivedMessageFrom(node2, node1.address,
                                          MessageType::DATA);
        });

    EXPECT_TRUE(received) << "Node2 did not receive message from Node1";

    // Verify message content
    auto messages =
        GetReceivedMessages(node2, node1.address, MessageType::DATA);
    ASSERT_EQ(messages.size(), 1) << "Expected exactly 1 message";

    auto msg_payload = messages[0].GetPayload();
    EXPECT_TRUE(std::ranges::equal(msg_payload, payload))
        << "Message payload mismatch";
}

/**
 * @brief Test three-node chain routing
 *
 * Topology: N1 <-> N2 <-> N3
 *
 * Verifies:
 * - N1 can reach N3 via N2 (2 hops)
 * - Routing tables have correct hop counts
 * - Message delivery works across 2 hops
 */
TEST_F(BasicRoutingTests, ThreeNodeChain) {
    // Create line topology with 3 nodes (first node is network manager)
    auto nodes = GenerateLineTopology(3, 0x1000, "Node", 0);

    ASSERT_EQ(nodes.size(), 3) << "Expected 3 nodes";

    // Start all nodes
    for (auto* node : nodes) {
        ASSERT_TRUE(StartNode(*node)) << "Failed to start " << node->name;
    }

    // Wait for network formation (2 normal nodes + 1 network manager)
    // Longer timeout for multi-hop joining with explicit roles
    ASSERT_TRUE(WaitForNetworkFormation(nodes, 2))
        << "Network formation failed";

    // Wait for routing tables to stabilize
    ASSERT_TRUE(WaitForRoutingStabilization(nodes))
        << "Routing stabilization failed";

    // Print routing tables for debugging
    for (auto* node : nodes) {
        PrintRoutingTable(*node);
    }

    // Verify Node1's routing table
    EXPECT_TRUE(HasRouteTo(*nodes[0], nodes[1]->address))
        << "Node1 should have route to Node2";
    EXPECT_EQ(GetHopCount(*nodes[0], nodes[1]->address), 1)
        << "Node1->Node2 should be 1 hop";

    EXPECT_TRUE(HasRouteTo(*nodes[0], nodes[2]->address))
        << "Node1 should have route to Node3";
    EXPECT_EQ(GetHopCount(*nodes[0], nodes[2]->address), 2)
        << "Node1->Node3 should be 2 hops";

    // Verify Node1's next hop to Node3 is Node2
    EXPECT_EQ(GetNextHop(*nodes[0], nodes[2]->address), nodes[1]->address)
        << "Node1 should route to Node3 via Node2";

    // Verify Node3's routing table
    EXPECT_TRUE(HasRouteTo(*nodes[2], nodes[1]->address))
        << "Node3 should have route to Node2";
    EXPECT_EQ(GetHopCount(*nodes[2], nodes[1]->address), 1)
        << "Node3->Node2 should be 1 hop";

    EXPECT_TRUE(HasRouteTo(*nodes[2], nodes[0]->address))
        << "Node3 should have route to Node1";
    EXPECT_EQ(GetHopCount(*nodes[2], nodes[0]->address), 2)
        << "Node3->Node1 should be 2 hops";

    // Send message from Node1 to Node3 (requires 2-hop routing)
    std::vector<uint8_t> payload = {0xAA, 0xBB, 0xCC};
    ASSERT_TRUE(SendMessage(*nodes[0], *nodes[2], payload))
        << "Failed to send message";

    auto superframe_duration = GetSuperframeDuration(*nodes[0]);
    uint32_t step_ms = 15u;

    // Wait for message to be received
    bool received =
        AdvanceTime(5000, superframe_duration * 3, step_ms, 0, [&]() {
            return HasReceivedMessageFrom(*nodes[2], nodes[0]->address,
                                          MessageType::DATA);
        });

    EXPECT_TRUE(received)
        << "Node3 did not receive message from Node1 through 2-hop routing";

    // Verify the message
    if (received) {
        auto messages = GetReceivedMessages(*nodes[2], nodes[0]->address,
                                            MessageType::DATA);
        ASSERT_EQ(messages.size(), 1) << "Expected exactly 1 message";
        EXPECT_TRUE(std::ranges::equal(messages[0].GetPayload(), payload))
            << "Message payload mismatch";
    }
}

}  // namespace test
}  // namespace loramesher
