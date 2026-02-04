/**
 * @file data_header.hpp
 * @brief Header definition for data messages with next-hop routing
 */

#pragma once

#include "types/messages/base_header.hpp"

namespace loramesher {

/**
 * @brief Header for DATA messages with next-hop routing support
 *
 * Extends BaseHeader with a next_hop field for multi-hop routing.
 * The next_hop field indicates the immediate recipient, while the
 * destination in BaseHeader indicates the final destination.
 *
 * Wire format (9 bytes total):
 * - Bytes 0-6: BaseHeader (dest, src, type=0x11, payload_size)
 * - Bytes 7-8: next_hop (little-endian)
 */
class DataHeader : public BaseHeader {
   public:
    /**
     * @brief Default constructor
     */
    DataHeader() = default;

    /**
     * @brief Constructor with all fields
     *
     * @param dest Final destination address
     * @param src Source address (original sender)
     * @param next_hop Next hop address for routing
     * @param payload_size Size of the data payload
     */
    DataHeader(AddressType dest, AddressType src, AddressType next_hop,
               uint8_t payload_size);

    /**
     * @brief Gets the next hop address for routing
     *
     * @return AddressType Next hop address
     */
    AddressType GetNextHop() const { return next_hop_; }

    /**
     * @brief Sets the next hop address
     *
     * @param next_hop New next hop address
     * @return Result Success if setting succeeded, error code otherwise
     */
    Result SetNextHop(AddressType next_hop);

    /**
     * @brief Serializes the header to a byte serializer
     *
     * Extends the base header serialization with data header specific fields.
     *
     * @param serializer Serializer to write the header to
     * @return Result Success if serialization succeeded, error code otherwise
     */
    Result Serialize(utils::ByteSerializer& serializer) const override;

    /**
     * @brief Deserializes a data header from a byte deserializer
     *
     * @param deserializer Deserializer containing the header data
     * @return std::optional<DataHeader> Deserialized header if successful,
     *         std::nullopt otherwise
     */
    static std::optional<DataHeader> Deserialize(
        utils::ByteDeserializer& deserializer);

    /**
     * @brief Calculates the size of the data header specific extension
     *
     * @return size_t Size of the data header fields in bytes (2 bytes for next_hop)
     */
    static constexpr size_t DataFieldsSize() {
        return sizeof(AddressType);  // next_hop
    }

    /**
     * @brief Gets the total size of this header type
     *
     * @return size_t Size of the header in bytes (base + data specific fields)
     */
    size_t GetSize() const override {
        return BaseHeader::Size() + DataFieldsSize();
    }

   private:
    AddressType next_hop_ = 0;  ///< Next hop address for routing
};

}  // namespace loramesher
