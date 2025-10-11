#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <atomic>

#include <zpp/core/inspection/namespace.h>
#include <zpp/types.h>
NSB_INSPECTION

struct mission {
    // Packed info word: [ tid:8 | steps:8 | reserved:16 | mid:32 ]
    uint64_t info;

    duration_t elapsed[]; //< time elapsed per step
};

// One-shot unpack result
struct mission_info {
    uint8_t tid;
    uint8_t steps;
    uint16_t reserved;
    uint32_t mid;
};

// Portable helpers to pack/unpack the info word.
inline constexpr uint64_t pack_mission_info(const mission_info& mi) noexcept {
    return (static_cast<uint64_t>(mi.mid) << 32) |
           (static_cast<uint64_t>(mi.reserved) << 16) |
           (static_cast<uint64_t>(mi.steps) << 8) |
           static_cast<uint64_t>(mi.tid);
}

// Efficient one-shot unpack: returns all fields in a POD struct. This inlines
// to exactly the same shifts/masks as calling the individual accessors.
[[nodiscard]] inline mission_info unpack_mission_info(const uint64_t info) noexcept {
    return mission_info{
        static_cast<uint8_t>(info & 0xFFu),
        static_cast<uint8_t>((info >> 8) & 0xFFu),
        static_cast<uint16_t>((info >> 16) & 0xFFFFu),
        static_cast<uint32_t>((info >> 32) & 0xFFFFFFFFu)
    };
}

// Sanity checks
static_assert(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");
static_assert(std::is_standard_layout_v<mission>, "mission must be standard-layout for offsetof to be valid");
static_assert(alignof(mission) >= alignof(duration_t), "mission must be at least duration_t aligned");
static_assert(offsetof(mission, elapsed) % alignof(duration_t) == 0, "elapsed[] must start at duration_t alignment");

NSE_INSPECTION