/**
 * @file test/test_os/rtos_eager_init_test.cpp
 * @brief Regression: GetRTOS() works without an explicit Init() call.
 *
 * The test main has no Init() call. If a future change re-introduces a
 * lazy/asserting RTOS::instance() (the f33f01d contract) without also
 * re-adding the test main's Init() warm-up, the first GetRTOS() call
 * here aborts and the test fails.
 *
 * The library's actual bug is application-level pre-Build() calls:
 * static LoraMesher::GenerateAddressFromHardware() and the global
 * `Logger LOG` reach GetRTOS() before any LoraMesher is constructed.
 * These tests cover both paths.
 */
#include <gtest/gtest.h>

#include "loramesher.hpp"
#include "os/os_port.hpp"
#include "utils/logger.hpp"

using namespace loramesher;

TEST(RTOSEagerInit, GetRTOSCallableDirectly) {
    auto& rtos = GetRTOS();
    (void)rtos.getTickCount();
    SUCCEED();
}

TEST(RTOSEagerInit, LogBeforeAnyLoraMesher) {
    LOG_WARNING("regression: log before any LoraMesher exists");
    SUCCEED();
}

TEST(RTOSEagerInit, GenerateAddressBeforeAnyLoraMesher) {
    AddressType address = LoraMesher::GenerateAddressFromHardware();
    (void)address;
    SUCCEED();
}
