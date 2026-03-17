/**
 * @file rtos_task_state_string_test.cpp
 * @brief Coverage tests for RTOS::getTaskStateString() switch branches.
 */

#include <gtest/gtest.h>

#ifdef ARDUINO

TEST(RTOSTaskStateStringTest, SkipOnArduino) {
    GTEST_SKIP();
}

#else

#include "os/rtos.hpp"

using namespace loramesher::os;

TEST(RTOSTaskStateStringTest, RunningState) {
    EXPECT_STREQ(RTOS::getTaskStateString(TaskState::kRunning), "Running");
}

TEST(RTOSTaskStateStringTest, ReadyState) {
    EXPECT_STREQ(RTOS::getTaskStateString(TaskState::kReady), "Ready");
}

TEST(RTOSTaskStateStringTest, BlockedState) {
    EXPECT_STREQ(RTOS::getTaskStateString(TaskState::kBlocked), "Blocked");
}

TEST(RTOSTaskStateStringTest, SuspendedState) {
    EXPECT_STREQ(RTOS::getTaskStateString(TaskState::kSuspended), "Suspended");
}

TEST(RTOSTaskStateStringTest, DeletedState) {
    EXPECT_STREQ(RTOS::getTaskStateString(TaskState::kDeleted), "Deleted");
}

TEST(RTOSTaskStateStringTest, UnknownState) {
    EXPECT_STREQ(RTOS::getTaskStateString(TaskState::kUnknown), "Unknown");
}

TEST(RTOSTaskStateStringTest, DefaultBranch) {
    // Cast an invalid value to trigger the default branch
    auto invalid = static_cast<TaskState>(99);
    EXPECT_STREQ(RTOS::getTaskStateString(invalid), "Unknown");
}

#endif  // ARDUINO
