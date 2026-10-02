/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NODESTREAMPROTOCOL_H
#define NODESTREAMPROTOCOL_H

#include <bit>
#include <cstddef>
#include <cstdint>

// Wire format for streaming GPU block-compressed layer textures from the master
// to the nodes over UDP multicast. Every datagram is self-describing: a fixed
// header followed by whole compressed blocks. A packet is either a segment of a
// single block row or a run of complete block rows, so a frame with lost packets
// can still be uploaded region by region.
namespace nodestream {

static_assert(std::endian::native == std::endian::little, "Node stream protocol assumes a little-endian host");

constexpr uint32_t kMagic = 0x534E5043; // "CPNS"
constexpr uint8_t kVersion = 1;
constexpr int kMaxDimension = 16384;
constexpr size_t kMaxDatagram = 65507;
constexpr uint16_t kDefaultPort = 50100;
constexpr const char *kDefaultGroup = "239.192.77.1";

enum class Format : uint8_t {
    Auto = 0,
    BC1 = 1,
    BC3 = 2,
    BC7 = 3
};

enum class SyncMode : uint8_t {
    FrameLocked = 0,
    Immediate = 1
};

#pragma pack(push, 1)
struct PacketHeader {
    uint32_t magic;
    uint8_t version;
    uint8_t format;
    uint16_t flags;
    uint32_t streamId;
    uint32_t sessionId;
    uint32_t frameId;
    uint16_t width;
    uint16_t height;
    uint32_t packetIndex;
    uint32_t packetCount;
    uint32_t byteOffset;
    uint32_t byteLength;
    uint64_t timestampNs;
};
#pragma pack(pop)
static_assert(sizeof(PacketHeader) == 48, "PacketHeader must stay 48 bytes on the wire");

inline bool isConcreteFormat(uint8_t f) {
    return f == static_cast<uint8_t>(Format::BC1) || f == static_cast<uint8_t>(Format::BC3)
           || f == static_cast<uint8_t>(Format::BC7);
}

inline uint32_t blockBytes(Format f) {
    return f == Format::BC1 ? 8u : 16u;
}

inline uint32_t blocksAcross(int pixels) {
    return static_cast<uint32_t>((pixels + 3) / 4);
}

inline size_t frameBytes(Format f, int width, int height) {
    return static_cast<size_t>(blocksAcross(width)) * blocksAcross(height) * blockBytes(f);
}

} // namespace nodestream

#endif // NODESTREAMPROTOCOL_H
