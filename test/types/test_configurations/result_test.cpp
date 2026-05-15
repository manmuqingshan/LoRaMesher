/**
 * @file result_test.cpp
 * @brief Unit tests for Result class and LoraMesherErrorCode enum
 */

#include <gtest/gtest.h>

#include "types/error_codes/result.hpp"

namespace loramesher {
namespace test {

// ---- Result construction ----

TEST(ResultTest, DefaultConstructorIsSuccess) {
    Result r;
    EXPECT_TRUE(r.IsSuccess());
    EXPECT_TRUE(static_cast<bool>(r));
    EXPECT_EQ(r.GetErrorCount(), 0u);
}

TEST(ResultTest, SuccessStaticFactory) {
    Result r = Result::Success();
    EXPECT_TRUE(r.IsSuccess());
    EXPECT_TRUE(r);
    EXPECT_EQ(r.GetErrorMessage(), "Success");
}

TEST(ResultTest, ErrorCodeConstructorSuccess) {
    // Constructing with kSuccess should produce a success result
    Result r(LoraMesherErrorCode::kSuccess);
    EXPECT_TRUE(r.IsSuccess());
}

TEST(ResultTest, ErrorCodeConstructor) {
    Result r(LoraMesherErrorCode::kTimeout);
    EXPECT_FALSE(r.IsSuccess());
    EXPECT_FALSE(static_cast<bool>(r));
    EXPECT_EQ(r.getErrorCode(), LoraMesherErrorCode::kTimeout);
}

TEST(ResultTest, ErrorCodeWithMessageConstructor) {
    Result r(LoraMesherErrorCode::kHardwareError, "custom message");
    EXPECT_FALSE(r.IsSuccess());
    EXPECT_EQ(r.getErrorCode(), LoraMesherErrorCode::kHardwareError);
    EXPECT_EQ(r.GetErrorMessage(), "custom message");
}

TEST(ResultTest, ErrorStaticFactory) {
    Result r = Result::Error(LoraMesherErrorCode::kNotInitialized);
    EXPECT_FALSE(r.IsSuccess());
    EXPECT_EQ(r.getErrorCode(), LoraMesherErrorCode::kNotInitialized);
}

TEST(ResultTest, InvalidArgumentFactory) {
    Result r = Result::InvalidArgument("bad arg");
    EXPECT_FALSE(r.IsSuccess());
    EXPECT_EQ(r.getErrorCode(), LoraMesherErrorCode::kInvalidArgument);
    EXPECT_EQ(r.GetErrorMessage(), "bad arg");
}

// ---- AddError ----

TEST(ResultTest, AddErrorSetsHasErrors) {
    Result r;
    EXPECT_TRUE(r.IsSuccess());
    r.AddError(LoraMesherErrorCode::kCrcError, "CRC failed");
    EXPECT_FALSE(r.IsSuccess());
    EXPECT_EQ(r.GetErrorCount(), 1u);
}

TEST(ResultTest, AddMultipleErrors) {
    Result r;
    r.AddError(LoraMesherErrorCode::kTimeout, "timeout 1");
    r.AddError(LoraMesherErrorCode::kBusyError, "busy");
    EXPECT_EQ(r.GetErrorCount(), 2u);

    // Primary error is the first one
    EXPECT_EQ(r.getErrorCode(), LoraMesherErrorCode::kTimeout);
}

TEST(ResultTest, GetErrorMessageMultipleErrors) {
    Result r;
    r.AddError(LoraMesherErrorCode::kTimeout, "line1");
    r.AddError(LoraMesherErrorCode::kBusyError, "line2");

    std::string msg = r.GetErrorMessage();
    EXPECT_NE(msg.find("line1"), std::string::npos);
    EXPECT_NE(msg.find("line2"), std::string::npos);
}

// ---- HasError ----

TEST(ResultTest, HasErrorFindsPresent) {
    Result r(LoraMesherErrorCode::kBufferOverflow, "overflow");
    EXPECT_TRUE(r.HasError(LoraMesherErrorCode::kBufferOverflow));
    EXPECT_FALSE(r.HasError(LoraMesherErrorCode::kTimeout));
}

TEST(ResultTest, HasErrorOnSuccess) {
    Result r;
    EXPECT_FALSE(r.HasError(LoraMesherErrorCode::kTimeout));
}

// ---- GetAllErrorCodes ----

TEST(ResultTest, GetAllErrorCodes) {
    Result r;
    r.AddError(LoraMesherErrorCode::kConfigurationError, "cfg");
    r.AddError(LoraMesherErrorCode::kTransmissionError, "tx");

    auto codes = r.GetAllErrorCodes();
    ASSERT_EQ(codes.size(), 2u);
    EXPECT_EQ(codes[0], LoraMesherErrorCode::kConfigurationError);
    EXPECT_EQ(codes[1], LoraMesherErrorCode::kTransmissionError);
}

TEST(ResultTest, GetAllErrorCodesEmpty) {
    Result r;
    EXPECT_TRUE(r.GetAllErrorCodes().empty());
}

// ---- MergeErrors ----

TEST(ResultTest, MergeErrorsFromSuccess) {
    Result base(LoraMesherErrorCode::kTimeout, "t");
    Result other;  // success
    base.MergeErrors(other);
    EXPECT_EQ(base.GetErrorCount(), 1u);
}

TEST(ResultTest, MergeErrorsFromError) {
    Result base;
    Result other(LoraMesherErrorCode::kHardwareError, "hw");
    base.MergeErrors(other);
    EXPECT_FALSE(base.IsSuccess());
    EXPECT_EQ(base.GetErrorCount(), 1u);
    EXPECT_EQ(base.getErrorCode(), LoraMesherErrorCode::kHardwareError);
}

TEST(ResultTest, MergeErrorsCombines) {
    Result base(LoraMesherErrorCode::kTimeout, "t");
    Result other(LoraMesherErrorCode::kBusyError, "b");
    base.MergeErrors(other);
    EXPECT_EQ(base.GetErrorCount(), 2u);
}

// ---- AsErrorCode ----

TEST(ResultTest, AsErrorCodeSuccess) {
    Result r;
    auto ec = r.AsErrorCode();
    EXPECT_EQ(ec.value(), 0);
}

TEST(ResultTest, AsErrorCodeError) {
    Result r(LoraMesherErrorCode::kReceptionError, "rx");
    auto ec = r.AsErrorCode();
    EXPECT_NE(ec.value(), 0);
}

// ---- Error category messages ----

TEST(ResultTest, ErrorCategoryMessageSuccess) {
    const auto& cat = LoraMesherErrorCategory::GetInstance();
    EXPECT_EQ(cat.message(static_cast<int>(LoraMesherErrorCode::kSuccess)),
              "Success");
}

TEST(ResultTest, ErrorCategoryAllKnownCodes) {
    const auto& cat = LoraMesherErrorCategory::GetInstance();

    // Verify ALL defined error codes have non-empty messages
    EXPECT_NE(cat.message(static_cast<int>(LoraMesherErrorCode::kUnknownError)),
              "");
    EXPECT_NE(
        cat.message(static_cast<int>(LoraMesherErrorCode::kConfigurationError)),
        "");
    EXPECT_NE(
        cat.message(static_cast<int>(LoraMesherErrorCode::kTransmissionError)),
        "");
    EXPECT_NE(
        cat.message(static_cast<int>(LoraMesherErrorCode::kReceptionError)),
        "");
    EXPECT_NE(cat.message(static_cast<int>(LoraMesherErrorCode::kInvalidState)),
              "");
    EXPECT_NE(
        cat.message(static_cast<int>(LoraMesherErrorCode::kHardwareError)), "");
    EXPECT_NE(cat.message(static_cast<int>(LoraMesherErrorCode::kTimeout)), "");
    EXPECT_NE(
        cat.message(static_cast<int>(LoraMesherErrorCode::kInvalidParameter)),
        "");
    EXPECT_NE(
        cat.message(static_cast<int>(LoraMesherErrorCode::kBufferOverflow)),
        "");
    EXPECT_NE(
        cat.message(static_cast<int>(LoraMesherErrorCode::kNotInitialized)),
        "");
    EXPECT_NE(cat.message(static_cast<int>(LoraMesherErrorCode::kCrcError)),
              "");
    EXPECT_NE(
        cat.message(static_cast<int>(LoraMesherErrorCode::kPreambleError)), "");
    EXPECT_NE(
        cat.message(static_cast<int>(LoraMesherErrorCode::kSyncWordError)), "");
    EXPECT_NE(
        cat.message(static_cast<int>(LoraMesherErrorCode::kFrequencyError)),
        "");
    EXPECT_NE(
        cat.message(static_cast<int>(LoraMesherErrorCode::kCalibrationError)),
        "");
    EXPECT_NE(cat.message(static_cast<int>(LoraMesherErrorCode::kMemoryError)),
              "");
    EXPECT_NE(cat.message(static_cast<int>(LoraMesherErrorCode::kBusyError)),
              "");
    EXPECT_NE(
        cat.message(static_cast<int>(LoraMesherErrorCode::kInterruptError)),
        "");
    EXPECT_NE(
        cat.message(static_cast<int>(LoraMesherErrorCode::kModulationError)),
        "");
    EXPECT_NE(
        cat.message(static_cast<int>(LoraMesherErrorCode::kInvalidArgument)),
        "");
    EXPECT_NE(
        cat.message(static_cast<int>(LoraMesherErrorCode::kNotImplemented)),
        "");
    EXPECT_NE(
        cat.message(static_cast<int>(LoraMesherErrorCode::kSerializationError)),
        "");
    EXPECT_NE(cat.message(static_cast<int>(LoraMesherErrorCode::kNoRoute)), "");
}

TEST(ResultTest, ErrorCategoryExactMessages) {
    const auto& cat = LoraMesherErrorCategory::GetInstance();

    // Verify the exact messages for recently-added error codes
    EXPECT_EQ(
        cat.message(static_cast<int>(LoraMesherErrorCode::kInvalidArgument)),
        "Invalid argument provided");
    EXPECT_EQ(
        cat.message(static_cast<int>(LoraMesherErrorCode::kNotImplemented)),
        "Feature not implemented");
    EXPECT_EQ(
        cat.message(static_cast<int>(LoraMesherErrorCode::kSerializationError)),
        "Message serialization failed");
    EXPECT_EQ(cat.message(static_cast<int>(LoraMesherErrorCode::kNoRoute)),
              "No route to destination");
    EXPECT_EQ(cat.message(static_cast<int>(LoraMesherErrorCode::kSuccess)),
              "Success");
}

TEST(ResultTest, ErrorCategoryUnknownCodeReturnsDefault) {
    const auto& cat = LoraMesherErrorCategory::GetInstance();
    // Value beyond the enum range hits the default case
    EXPECT_EQ(cat.message(9999), "Unknown error");
}

TEST(ResultTest, ErrorCategoryName) {
    const auto& cat = LoraMesherErrorCategory::GetInstance();
    EXPECT_NE(std::string(cat.name()), "");
}

TEST(ResultTest, ErrorCategoryIsSingleton) {
    const auto& a = LoraMesherErrorCategory::GetInstance();
    const auto& b = LoraMesherErrorCategory::GetInstance();
    EXPECT_EQ(&a, &b);
}

// ---- ErrorInfo construction ----

TEST(ErrorInfoTest, Construction) {
    ErrorInfo info(LoraMesherErrorCode::kNoRoute, "no route to host");
    EXPECT_EQ(info.code, LoraMesherErrorCode::kNoRoute);
    EXPECT_EQ(info.message, "no route to host");
}

}  // namespace test
}  // namespace loramesher
