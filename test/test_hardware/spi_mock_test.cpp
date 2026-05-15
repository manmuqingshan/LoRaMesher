/**
 * @file spi_mock_test.cpp
 * @brief Tests for SPIMock (SPIClass mock implementation for native builds)
 */
#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>

#include "hardware/SPIMock.hpp"

namespace loramesher {
namespace test {

class SPIMockTest : public ::testing::Test {
   protected:
    SPIClass spi_{0};
};

TEST_F(SPIMockTest, ConstructWithBusNumber) {
    SPIClass spi0(0);
    SPIClass spi1(1);
    SPIClass spi2(2);
    // Construction should not crash
}

TEST_F(SPIMockTest, BeginAndEnd) {
    spi_.begin();
    spi_.end();
}

TEST_F(SPIMockTest, BeginWithCustomPins) {
    spi_.begin(18, 19, 23, 5);
    spi_.end();
}

TEST_F(SPIMockTest, TransferSingleByteBeforeInit) {
    // Transfer before begin() should return 0xFF
    uint8_t result = spi_.transfer(0x42);
    EXPECT_EQ(result, 0xFF);
}

TEST_F(SPIMockTest, TransferSingleByteAfterInit) {
    spi_.begin();
    uint8_t result = spi_.transfer(0x42);
    EXPECT_EQ(result, 0xA5);
    spi_.end();
}

TEST_F(SPIMockTest, TransferMultiBytesBeforeInit) {
    uint8_t data[] = {0x01, 0x02, 0x03};
    // Should not crash, just warn
    spi_.transfer(data, sizeof(data));
}

TEST_F(SPIMockTest, TransferMultiBytesAfterInit) {
    spi_.begin();
    uint8_t data[] = {0x01, 0x02, 0x03};
    spi_.transfer(data, sizeof(data));
    spi_.end();
}

TEST_F(SPIMockTest, TransferBytesBeforeInit) {
    uint8_t tx[] = {0x01, 0x02, 0x03};
    uint8_t rx[3] = {};
    // Should not crash, just warn
    spi_.transferBytes(tx, rx, sizeof(tx));
    // rx should be unchanged since not initialized
    EXPECT_EQ(rx[0], 0);
    EXPECT_EQ(rx[1], 0);
    EXPECT_EQ(rx[2], 0);
}

TEST_F(SPIMockTest, TransferBytesAfterInit) {
    spi_.begin();
    uint8_t tx[] = {0x01, 0x02, 0x03};
    uint8_t rx[3] = {};
    spi_.transferBytes(tx, rx, sizeof(tx));
    // Mock inverts the data (XOR 0xFF)
    EXPECT_EQ(rx[0], 0x01 ^ 0xFF);
    EXPECT_EQ(rx[1], 0x02 ^ 0xFF);
    EXPECT_EQ(rx[2], 0x03 ^ 0xFF);
    spi_.end();
}

TEST_F(SPIMockTest, TransferBytesNullBuffers) {
    spi_.begin();
    // Should not crash with null rx buffer
    uint8_t tx[] = {0x01};
    spi_.transferBytes(tx, nullptr, 1);
    // Should not crash with null tx buffer
    uint8_t rx[1] = {};
    spi_.transferBytes(nullptr, rx, 1);
    spi_.end();
}

TEST_F(SPIMockTest, SetBitOrder) {
    spi_.setBitOrder(MSBFIRST);
    spi_.setBitOrder(LSBFIRST);
}

TEST_F(SPIMockTest, SetClockDivider) {
    spi_.setClockDivider(2);
    spi_.setClockDivider(4);
}

TEST_F(SPIMockTest, SetDataMode) {
    spi_.setDataMode(SPI_MODE0);
    spi_.setDataMode(SPI_MODE1);
    spi_.setDataMode(SPI_MODE2);
    spi_.setDataMode(SPI_MODE3);
}

TEST_F(SPIMockTest, SetFrequency) {
    spi_.setFrequency(1000000);
    spi_.setFrequency(8000000);
}

TEST_F(SPIMockTest, GlobalSPIInstances) {
    // Verify the global SPI instances exist and can be used
    SPI.begin();
    SPI.end();
    SPI1.begin();
    SPI1.end();
    SPI2.begin();
    SPI2.end();
}

TEST_F(SPIMockTest, SPIModeConstants) {
    EXPECT_EQ(MSBFIRST, 0);
    EXPECT_EQ(LSBFIRST, 1);
    EXPECT_EQ(SPI_MODE0, 0);
    EXPECT_EQ(SPI_MODE1, 1);
    EXPECT_EQ(SPI_MODE2, 2);
    EXPECT_EQ(SPI_MODE3, 3);
}

}  // namespace test
}  // namespace loramesher
