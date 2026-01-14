/**
 * @file capability_propagation_test.cpp
 * @brief Test suite for node capabilities propagation through the mesh network
 */

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "lora_mesh_test_fixture.hpp"
#include "types/node_capabilities.hpp"

namespace loramesher {
namespace test {

/**
 * @brief Test suite for node capabilities propagation
 *
 * Verifies that node capabilities are properly propagated through routing
 * table messages and can be queried by all nodes in the network.
 */
class CapabilityPropagationTests : public LoRaMeshTestFixture {
   protected:
    void SetUp() override { LoRaMeshTestFixture::SetUp(); }

    void TearDown() override { LoRaMeshTestFixture::TearDown(); }

    /**
     * @brief Wait for tasks to execute
     */
    void WaitForTasksToExecute() {
#ifdef LORAMESHER_BUILD_ARDUINO
        GetRTOS().delay(20);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
#endif
    }

    /**
     * @brief Wait for capabilities to propagate through the network
     *
     * @param nodes Nodes in the network
     * @param timeout_ms Timeout for waiting
     * @return bool True if all capabilities propagated, false on timeout
     */
    bool WaitForCapabilityPropagation(const std::vector<TestNode*>& nodes,
                                      uint32_t timeout_ms = 10000) {
        return AdvanceTime(timeout_ms, timeout_ms, 100, 2, [&]() {
            // Check if all nodes know about each other's capabilities
            for (auto* node : nodes) {
                for (auto* other_node : nodes) {
                    if (node->address != other_node->address) {
                        // Query capabilities of other node
                        uint8_t caps =
                            node->protocol->GetNodeCapabilities(other_node->address);

                        // If capabilities are 0 (unknown), propagation not complete
                        // unless the other node actually has 0 capabilities
                        auto expected_caps =
                            other_node->protocol->GetLocalNodeCapabilities();
                        if (caps != expected_caps) {
                            return false;
                        }
                    }
                }
            }
            return true;
        });
    }
};

/**
 * @brief Test basic capability propagation between two nodes
 *
 * Verifies that capabilities set on one node are propagated to another
 * node through routing table messages.
 */
TEST_F(CapabilityPropagationTests, CapabilityPropagationThroughNetwork) {
    // Create two nodes with different capabilities
    auto& node1 = CreateNode("Node1", 0x1001);
    auto& node2 = CreateNode("Node2", 0x1002);

    // Set link status
    SetLinkStatus(node1, node2, true);

    // Start node1 (will become network manager)
    ASSERT_TRUE(StartNode(node1));

    WaitForTasksToExecute();

    // Wait for node1 to become network manager
    auto discovery_timeout = GetDiscoveryTimeout(node1);
    auto slot_duration = GetSlotDuration(node1);

    bool became_manager = AdvanceTime(
        slot_duration / 2, discovery_timeout + 500, 10, 2, [&]() {
            return node1.protocol->GetState() ==
                   protocols::lora_mesh::INetworkService::ProtocolState::
                       NETWORK_MANAGER;
        });
    ASSERT_TRUE(became_manager) << "Node1 did not become network manager";

    // Set capabilities on node1
    node1.protocol->SetNodeCapabilities(ROUTER);

    WaitForTasksToExecute();

    // Start node2
    ASSERT_TRUE(StartNode(node2));

    WaitForTasksToExecute();

    // Wait for node2 to join and reach NORMAL_OPERATION
    bool joined = AdvanceTime(slot_duration / 2, discovery_timeout + 5000, 10, 2,
                              [&]() {
                                  return node2.protocol->GetState() ==
                                         protocols::lora_mesh::INetworkService::
                                             ProtocolState::NORMAL_OPERATION;
                              });
    ASSERT_TRUE(joined) << "Node2 did not join network";

    // Set capabilities on node2
    node2.protocol->SetNodeCapabilities(GATEWAY | SENSOR_NODE);

    WaitForTasksToExecute();

    // Wait for capabilities to propagate
    std::vector<TestNode*> nodes = {&node1, &node2};
    bool propagated = WaitForCapabilityPropagation(nodes, 15000);

    ASSERT_TRUE(propagated) << "Capabilities did not propagate in time";

    // Verify node1 can query node2's capabilities
    uint8_t node2_caps = node1.protocol->GetNodeCapabilities(node2.address);
    EXPECT_EQ(node2_caps, GATEWAY | SENSOR_NODE)
        << "Node1 should know Node2's capabilities";

    // Verify node2 can query node1's capabilities
    uint8_t node1_caps = node2.protocol->GetNodeCapabilities(node1.address);
    EXPECT_EQ(node1_caps, ROUTER) << "Node2 should know Node1's capabilities";

    // Verify local capabilities are correct
    EXPECT_EQ(node1.protocol->GetLocalNodeCapabilities(), ROUTER);
    EXPECT_EQ(node2.protocol->GetLocalNodeCapabilities(), GATEWAY | SENSOR_NODE);
}

/**
 * @brief Test capability update propagation
 *
 * Verifies that when a node updates its capabilities at runtime,
 * the updates are propagated to other nodes.
 */
TEST_F(CapabilityPropagationTests, CapabilityUpdatePropagation) {
    // Create two nodes
    auto& node1 = CreateNode("Node1", 0x1001);
    auto& node2 = CreateNode("Node2", 0x1002);

    SetLinkStatus(node1, node2, true);

    // Start node1 with ROUTER capability
    ASSERT_TRUE(StartNode(node1));
    node1.protocol->SetNodeCapabilities(ROUTER);

    WaitForTasksToExecute();

    // Wait for node1 to become network manager
    auto discovery_timeout = GetDiscoveryTimeout(node1);
    auto slot_duration = GetSlotDuration(node1);

    bool became_manager = AdvanceTime(
        slot_duration / 2, discovery_timeout + 500, 10, 2, [&]() {
            return node1.protocol->GetState() ==
                   protocols::lora_mesh::INetworkService::ProtocolState::
                       NETWORK_MANAGER;
        });
    ASSERT_TRUE(became_manager);

    // Start node2
    ASSERT_TRUE(StartNode(node2));

    WaitForTasksToExecute();

    // Wait for node2 to join
    bool joined = AdvanceTime(slot_duration / 2, discovery_timeout + 5000, 10, 2,
                              [&]() {
                                  return node2.protocol->GetState() ==
                                         protocols::lora_mesh::INetworkService::
                                             ProtocolState::NORMAL_OPERATION;
                              });
    ASSERT_TRUE(joined);

    // Wait for initial capabilities to propagate
    std::vector<TestNode*> nodes = {&node1, &node2};
    bool initial_propagated = WaitForCapabilityPropagation(nodes, 15000);
    ASSERT_TRUE(initial_propagated);

    // Verify initial state
    uint8_t initial_caps = node2.protocol->GetNodeCapabilities(node1.address);
    EXPECT_EQ(initial_caps, ROUTER);

    // Update node1 capabilities at runtime
    node1.protocol->SetNodeCapabilities(ROUTER | BATTERY_POWERED);

    WaitForTasksToExecute();

    // Wait for updated capabilities to propagate
    bool updated_propagated = AdvanceTime(
        100, 15000, 100, 2, [&]() {
            uint8_t caps = node2.protocol->GetNodeCapabilities(node1.address);
            return caps == (ROUTER | BATTERY_POWERED);
        });

    ASSERT_TRUE(updated_propagated)
        << "Updated capabilities did not propagate in time";

    // Verify node2 received the updated capabilities
    uint8_t updated_caps = node2.protocol->GetNodeCapabilities(node1.address);
    EXPECT_EQ(updated_caps, ROUTER | BATTERY_POWERED)
        << "Node2 should see Node1's updated capabilities";

    // Verify node1's local capabilities are correct
    EXPECT_EQ(node1.protocol->GetLocalNodeCapabilities(),
              ROUTER | BATTERY_POWERED);
}

/**
 * @brief Test capability tracking in multi-node network
 *
 * Verifies that in a network with multiple nodes, each with different
 * capabilities, all nodes can correctly query each other's capabilities.
 */
TEST_F(CapabilityPropagationTests, MultiNodeCapabilityTracking) {
    // Create 4 nodes with different capabilities
    auto& node1 = CreateNode("Node1", 0x1001);
    auto& node2 = CreateNode("Node2", 0x1002);
    auto& node3 = CreateNode("Node3", 0x1003);
    auto& node4 = CreateNode("Node4", 0x1004);

    // Create a fully connected mesh
    SetLinkStatus(node1, node2, true);
    SetLinkStatus(node1, node3, true);
    SetLinkStatus(node1, node4, true);
    SetLinkStatus(node2, node3, true);
    SetLinkStatus(node2, node4, true);
    SetLinkStatus(node3, node4, true);

    // Start node1 (will become network manager)
    ASSERT_TRUE(StartNode(node1));
    node1.protocol->SetNodeCapabilities(ROUTER);

    WaitForTasksToExecute();

    // Wait for node1 to become network manager
    auto discovery_timeout = GetDiscoveryTimeout(node1);
    auto slot_duration = GetSlotDuration(node1);

    bool became_manager = AdvanceTime(
        slot_duration / 2, discovery_timeout + 500, 10, 2, [&]() {
            return node1.protocol->GetState() ==
                   protocols::lora_mesh::INetworkService::ProtocolState::
                       NETWORK_MANAGER;
        });
    ASSERT_TRUE(became_manager);

    // Start node2 with GATEWAY capability
    ASSERT_TRUE(StartNode(node2));
    node2.protocol->SetNodeCapabilities(GATEWAY);

    WaitForTasksToExecute();

    // Wait for node2 to join
    bool node2_joined = AdvanceTime(
        slot_duration / 2, discovery_timeout + 5000, 10, 2, [&]() {
            return node2.protocol->GetState() ==
                   protocols::lora_mesh::INetworkService::ProtocolState::
                       NORMAL_OPERATION;
        });
    ASSERT_TRUE(node2_joined);

    // Start node3 with SENSOR_NODE | BATTERY_POWERED capabilities
    ASSERT_TRUE(StartNode(node3));
    node3.protocol->SetNodeCapabilities(SENSOR_NODE | BATTERY_POWERED);

    WaitForTasksToExecute();

    // Wait for node3 to join
    bool node3_joined = AdvanceTime(
        slot_duration / 2, discovery_timeout + 5000, 10, 2, [&]() {
            return node3.protocol->GetState() ==
                   protocols::lora_mesh::INetworkService::ProtocolState::
                       NORMAL_OPERATION;
        });
    ASSERT_TRUE(node3_joined);

    // Start node4 with ROUTER | HIGH_BANDWIDTH capabilities
    ASSERT_TRUE(StartNode(node4));
    node4.protocol->SetNodeCapabilities(ROUTER | HIGH_BANDWIDTH);

    WaitForTasksToExecute();

    // Wait for node4 to join
    bool node4_joined = AdvanceTime(
        slot_duration / 2, discovery_timeout + 5000, 10, 2, [&]() {
            return node4.protocol->GetState() ==
                   protocols::lora_mesh::INetworkService::ProtocolState::
                       NORMAL_OPERATION;
        });
    ASSERT_TRUE(node4_joined);

    // Wait for all capabilities to propagate
    std::vector<TestNode*> nodes = {&node1, &node2, &node3, &node4};
    bool all_propagated = WaitForCapabilityPropagation(nodes, 20000);

    ASSERT_TRUE(all_propagated) << "Not all capabilities propagated in time";

    // Verify each node can query all other nodes' capabilities
    EXPECT_EQ(node1.protocol->GetNodeCapabilities(node2.address), GATEWAY);
    EXPECT_EQ(node1.protocol->GetNodeCapabilities(node3.address),
              SENSOR_NODE | BATTERY_POWERED);
    EXPECT_EQ(node1.protocol->GetNodeCapabilities(node4.address),
              ROUTER | HIGH_BANDWIDTH);

    EXPECT_EQ(node2.protocol->GetNodeCapabilities(node1.address), ROUTER);
    EXPECT_EQ(node2.protocol->GetNodeCapabilities(node3.address),
              SENSOR_NODE | BATTERY_POWERED);
    EXPECT_EQ(node2.protocol->GetNodeCapabilities(node4.address),
              ROUTER | HIGH_BANDWIDTH);

    EXPECT_EQ(node3.protocol->GetNodeCapabilities(node1.address), ROUTER);
    EXPECT_EQ(node3.protocol->GetNodeCapabilities(node2.address), GATEWAY);
    EXPECT_EQ(node3.protocol->GetNodeCapabilities(node4.address),
              ROUTER | HIGH_BANDWIDTH);

    EXPECT_EQ(node4.protocol->GetNodeCapabilities(node1.address), ROUTER);
    EXPECT_EQ(node4.protocol->GetNodeCapabilities(node2.address), GATEWAY);
    EXPECT_EQ(node4.protocol->GetNodeCapabilities(node3.address),
              SENSOR_NODE | BATTERY_POWERED);

    // Verify local capabilities are correct
    EXPECT_EQ(node1.protocol->GetLocalNodeCapabilities(), ROUTER);
    EXPECT_EQ(node2.protocol->GetLocalNodeCapabilities(), GATEWAY);
    EXPECT_EQ(node3.protocol->GetLocalNodeCapabilities(),
              SENSOR_NODE | BATTERY_POWERED);
    EXPECT_EQ(node4.protocol->GetLocalNodeCapabilities(),
              ROUTER | HIGH_BANDWIDTH);
}

/**
 * @brief Test querying capabilities of unknown node
 *
 * Verifies that querying capabilities of a non-existent node
 * returns 0 and doesn't crash.
 */
TEST_F(CapabilityPropagationTests, UnknownNodeCapabilityQuery) {
    // Create a single node
    auto& node = CreateNode("Node1", 0x1001);

    // Start the node
    ASSERT_TRUE(StartNode(node));

    WaitForTasksToExecute();

    // Wait for node to become network manager
    auto discovery_timeout = GetDiscoveryTimeout(node);
    auto slot_duration = GetSlotDuration(node);

    bool became_manager = AdvanceTime(
        slot_duration / 2, discovery_timeout + 500, 10, 2, [&]() {
            return node.protocol->GetState() ==
                   protocols::lora_mesh::INetworkService::ProtocolState::
                       NETWORK_MANAGER;
        });
    ASSERT_TRUE(became_manager);

    // Query capabilities of non-existent node
    AddressType unknown_address = 0x9999;
    uint8_t caps = node.protocol->GetNodeCapabilities(unknown_address);

    // Should return 0 (no capabilities) for unknown node
    EXPECT_EQ(caps, 0) << "Unknown node should have 0 capabilities";

    // Verify no crash occurred and we can continue using the node
    EXPECT_EQ(node.protocol->GetState(),
              protocols::lora_mesh::INetworkService::ProtocolState::
                  NETWORK_MANAGER);
}

}  // namespace test
}  // namespace loramesher
