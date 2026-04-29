/**
 * @file readiness_integration_test.cpp
 * @brief End-to-end contract tests for IsReadyToSend()
 *
 * Verifies that once IsReadyToSend()==true reports across a stabilized
 * topology, an immediate Send() / SendBroadcast() actually succeeds and
 * the message is delivered. This pins the predicate's *meaning*; lower-
 * level transitions are covered in the protocol-lifecycle unit suite.
 */

#include <gtest/gtest.h>

#include "routing_test_fixture.hpp"
#include "types/messages/base_header.hpp"

namespace loramesher {
namespace test {

class ReadinessIntegrationTests : public RoutingTestFixture {};

/**
 * @brief Positive end-to-end contract: IsReadyToSend ⇒ Send succeeds & delivers
 *
 * 3-node line. After stabilization, endpoint Node1 reports
 * IsReadyToSend(Node3)==true. An immediate unicast Send() must succeed and
 * the payload must reach Node3 across the multi-hop relay.
 */
TEST_F(ReadinessIntegrationTests, ReadyImpliesSendSucceedsEndToEnd) {
    auto nodes = GenerateLineTopology(3, 0x1000, "Node", 0);
    ASSERT_EQ(nodes.size(), 3u);

    for (auto* node : nodes) {
        ASSERT_TRUE(StartNode(*node)) << "Failed to start " << node->name;
    }
    ASSERT_TRUE(WaitForNetworkFormation(nodes, 2));
    ASSERT_TRUE(WaitForRoutingStabilization(nodes));

    {
        Result ready = nodes[0]->protocol->IsReadyToSend(nodes[2]->address);
        ASSERT_TRUE(ready)
            << "Pre-condition: Node1 should be ready to send to Node3 ("
            << ready.GetErrorMessage() << ")";
    }

    ClearAllReceivedMessages();

    std::vector<uint8_t> payload = {0xCA, 0xFE, 0xBA, 0xBE};
    Result send_result = SendMessage(*nodes[0], *nodes[2], payload);
    ASSERT_TRUE(send_result) << "Send() failed despite IsReadyToSend()==true: "
                             << send_result.GetErrorMessage();

    auto superframe_time = GetSuperframeDuration(*nodes.front());
    bool delivered =
        AdvanceTime(superframe_time * 4, superframe_time * 4, 50u, 0, [&]() {
            return HasReceivedMessageFrom(*nodes[2], nodes[0]->address,
                                          MessageType::DATA);
        });

    EXPECT_TRUE(delivered) << "Node3 did not receive payload from Node1 after "
                              "IsReadyToSend()==true";
}

/**
 * @brief Positive contract for broadcast: ready ⇒ SendBroadcast delivers
 */
TEST_F(ReadinessIntegrationTests, ReadyImpliesBroadcastSucceedsEndToEnd) {
    auto nodes = GenerateLineTopology(3, 0x1000, "Node", 0);
    ASSERT_EQ(nodes.size(), 3u);

    for (auto* node : nodes) {
        ASSERT_TRUE(StartNode(*node)) << "Failed to start " << node->name;
    }
    ASSERT_TRUE(WaitForNetworkFormation(nodes, 2));
    ASSERT_TRUE(WaitForRoutingStabilization(nodes));

    {
        Result ready = nodes[0]->protocol->IsReadyToSend(kBroadcastAddress);
        ASSERT_TRUE(ready) << "Pre-condition: Node1 should be ready for "
                              "broadcast ("
                           << ready.GetErrorMessage() << ")";
    }

    ClearAllReceivedMessages();

    std::vector<uint8_t> payload = {0xB1, 0xB2, 0xB3};
    Result bcast_result = nodes[0]->protocol->SendBroadcast(payload);
    ASSERT_TRUE(bcast_result)
        << "SendBroadcast() failed despite ready predicate: "
        << bcast_result.GetErrorMessage();

    auto superframe_time = GetSuperframeDuration(*nodes.front());
    bool all_received =
        AdvanceTime(superframe_time * 6, superframe_time * 6, 50u, 0, [&]() {
            for (size_t i = 1; i < nodes.size(); i++) {
                if (!HasReceivedMessageFrom(*nodes[i], nodes[0]->address,
                                            MessageType::DATA)) {
                    return false;
                }
            }
            return true;
        });

    EXPECT_TRUE(all_received)
        << "Broadcast did not reach all nodes after ready predicate";
}

/**
 * @brief Send the moment IsReadyToSend() flips: must deliver within ≤2 superframes
 *
 * The strict timing contract: once the predicate transitions from
 * non-Success to Success, the very next Send() should produce a delivered
 * message in the current superframe (if a TX slot remains) or the
 * following one (if readiness landed after this superframe's TX slot
 * already passed). Allowing a 2× margin avoids flakiness on the second
 * case while still pinning the contract: a node that becomes ready does
 * not need a multi-superframe warm-up before it can send.
 */
TEST_F(ReadinessIntegrationTests,
       SendImmediatelyAfterReadyDeliversWithinTwoSuperframes) {
    auto nodes = GenerateLineTopology(2, 0x1000, "Node", 0);
    ASSERT_EQ(nodes.size(), 2u);

    for (auto* node : nodes) {
        ASSERT_TRUE(StartNode(*node)) << "Failed to start " << node->name;
    }

    auto& sender = *nodes[1];
    auto& receiver = *nodes[0];

    auto first_superframe = GetSuperframeDuration(sender);
    if (first_superframe == 0) {
        first_superframe = GetSlotDuration(sender) * 64;
    }

    bool became_ready = AdvanceTime(
        first_superframe * 20, first_superframe * 20, 15u, 0, [&]() {
            return static_cast<bool>(
                sender.protocol->IsReadyToSend(receiver.address));
        });
    ASSERT_TRUE(became_ready)
        << sender.name << " never reached IsReadyToSend(receiver)==Success";

    ClearAllReceivedMessages();

    std::vector<uint8_t> payload = {0x11, 0x22, 0x33, 0x44};
    Result send_result = SendMessage(sender, receiver, payload);
    ASSERT_TRUE(send_result)
        << "Send() failed immediately after IsReadyToSend() flipped to "
           "Success: "
        << send_result.GetErrorMessage();

    auto superframe_time = GetSuperframeDuration(sender);
    ASSERT_GT(superframe_time, 0u);

    bool delivered_within_window =
        AdvanceTime(superframe_time * 2, superframe_time * 2, 15u, 0, [&]() {
            return HasReceivedMessageFrom(receiver, sender.address,
                                          MessageType::DATA);
        });

    EXPECT_TRUE(delivered_within_window)
        << "Message sent immediately after readiness must be delivered "
           "within 2 superframes (slot timing contract). Superframe="
        << superframe_time << "ms.";
}

}  // namespace test
}  // namespace loramesher
