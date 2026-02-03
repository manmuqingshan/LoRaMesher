/**
 * @file power_types.hpp
 * @brief Power management callback types for LoRaMesher
 */
#pragma once

#include <functional>

namespace loramesher {
namespace power {

/**
 * @brief Callback invoked before the device enters sleep mode
 *
 * This callback allows the application to perform any necessary
 * cleanup or state saving before the system enters low-power sleep.
 */
using PrepareSleepCallback = std::function<void()>;

/**
 * @brief Callback invoked when the device wakes up from sleep
 *
 * This callback allows the application to restore state or
 * perform any necessary initialization after waking from sleep.
 */
using WakeUpCallback = std::function<void()>;

}  // namespace power
}  // namespace loramesher
