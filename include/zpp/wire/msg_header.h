#pragma once

#include <cstdint>
#include <cstddef>

/**
 * @file msg_header.h
 *
 * @brief Compact binary message headers for efficient message routing.
 *
 * @note
 * This protocol uses small numeric fields instead of string-based headers.
 * The purpose is to make message parsing and dispatching fast.
 *
 * Basic routing can be done by reading only:
 *
 *   - version
 *   - category
 *   - type
 *
 * For PUB messages, topic information is stored immediately after pub_header.
 *
 * PUB message layout:
 *
 *   [base_header][topic_count][topic0...topicN][body...]
 *
 * The topic is represented as multiple uint8_t values.
 * Users should define a reasonable topic hierarchy by themselves.
 *
 * Users are also responsible for defining stable category/type values.
 *
 * Important:
 *
 *   - All messages must be length-checked before accessing variable data.
 *   - The byte order of multi-byte fields, such as uint16_t type, should be
 *     explicitly defined by the protocol.
 *   - If messages may cross machines, processes, languages, or architectures,
 *     do not rely on native host byte order.
 */

#pragma pack(push, 1)

/**
 * @brief Common fixed-size message header.
 *
 * This header is shared by all message types.
 *
 * Fixed binary layout:
 *
 *   offset  size  field
 *   ------  ----  --------
 *   0       1     version
 *   1       1     category
 *   2       2     type
 *
 * Total size: 4 bytes.
 *
 * The receiver can use category and type to quickly route the message
 * without decoding the full message body.
 */
struct base_header {
    std::uint8_t  version;   ///< Protocol/message version. Starts from 1.
    std::uint8_t  category;  ///< High-level message category.
    std::uint16_t type;      ///< Message subtype under the given category.
};

/**
 * @brief Header for PUB messages.
 *
 * PUB message layout:
 *
 *   [pub_header][topic0...topicN][body...]
 *
 * Since pub_header already contains base_header, the full layout is:
 *
 *   [version][category][type][topic_count][topics...][body...]
 *
 * The topic array is not stored as a C/C++ flexible array member.
 * Instead, it starts immediately after pub_header in the message buffer.
 *
 * This keeps the structure standard-friendly and avoids zero-length array
 * or flexible-array-member extensions.
 */
struct pub_header {
    base_header  base;        ///< Common base message header.
    std::uint8_t topic_count; ///< Number of uint8_t topic elements.
};

#pragma pack(pop)

static_assert(sizeof(base_header) == 4, "base_header must be exactly 4 bytes");
static_assert(sizeof(pub_header) == 5, "pub_header must be exactly 5 bytes");

namespace z::wire {

/**
 * @brief Returns the beginning of the PUB topic array.
 *
 * The returned pointer points to the first topic byte immediately after
 * pub_header.
 */
inline const std::uint8_t* pub_topic_begin(const pub_header* h) {
    return reinterpret_cast<const std::uint8_t*>(h) + sizeof(pub_header);
}

/**
 * @brief Returns the beginning of the PUB message body.
 *
 * The body starts immediately after topic_count topic bytes.
 *
 * Caller must ensure that the message buffer is valid and long enough.
 */
inline const std::uint8_t* pub_body_begin(const pub_header* h) {
    return pub_topic_begin(h) + h->topic_count;
}

/**
 * @brief Returns the total header size of a PUB message.
 *
 * This includes:
 *
 *   sizeof(pub_header) + topic_count
 *
 * It does not include the body size.
 */
inline std::size_t pub_header_size(const pub_header* h) {
    return sizeof(pub_header) + h->topic_count;
}

/**
 * @brief Checks whether a buffer contains a complete PUB header and topic area.
 *
 * This function only validates the fixed PUB header and topic bytes.
 * It does not validate the semantic correctness of version/category/type/topic.
 */
inline bool is_valid_pub_message(const void* data, std::size_t len) {
    if (data == nullptr) {
        return false;
    }

    if (len < sizeof(pub_header)) {
        return false;
    }

    const auto* h = reinterpret_cast<const pub_header*>(data);

    const std::size_t header_size = pub_header_size(h);
    if (len < header_size) {
        return false;
    }

    return true;
}

/**
 * @brief Returns the PUB body size.
 *
 * Caller must ensure that is_valid_pub_message(data, len) is true before
 * calling this function.
 */
inline std::size_t pub_body_size(const pub_header* h, std::size_t total_len) {
    return total_len - pub_header_size(h);
}

} // namespace wire