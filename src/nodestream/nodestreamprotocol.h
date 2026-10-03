/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NODESTREAMPROTOCOL_H
#define NODESTREAMPROTOCOL_H

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>

// Wire format for streaming GPU block-compressed layer textures from the master
// to the nodes over UDP multicast. Every datagram is self-describing: a fixed
// header followed by one unit of whole compressed blocks, optionally LZ4
// compressed. A unit is either a segment of a single block row or a run of
// complete block rows, so a frame with lost packets can still be uploaded
// region by region. A keyframe carries all units; a delta frame carries only
// the units that changed since its base frame.
namespace nodestream {

static_assert(std::endian::native == std::endian::little, "Node stream protocol assumes a little-endian host");

constexpr uint32_t kMagic = 0x534E5043; // "CPNS"
constexpr uint8_t kVersion = 2;
constexpr int kMaxDimension = 16384;
constexpr size_t kMaxDatagram = 65507;
constexpr uint16_t kDefaultPort = 50100;
constexpr const char *kDefaultGroup = "239.192.77.1";

// The payload is an LZ4 block that decompresses to byteLength bytes.
constexpr uint16_t kFlagLz4 = 0x1;

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
    // 0 for a keyframe, otherwise the frame this delta applies on top of.
    uint32_t baseFrameId;
    uint16_t width;
    uint16_t height;
    uint32_t packetIndex;
    uint32_t packetCount;
    uint32_t byteOffset;
    // Uncompressed length; the payload is the rest of the datagram.
    uint32_t byteLength;
    uint64_t timestampNs;
};
#pragma pack(pop)
static_assert(sizeof(PacketHeader) == 52, "PacketHeader must stay 52 bytes on the wire");

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

// How a frame is split into units, each sent as one datagram.
struct UnitLayout {
    uint32_t rowBytes = 0;
    uint32_t rows = 0;
    uint32_t maxPayload = 0;
    // Whole block rows per unit when a row fits, otherwise 0 and the rows are split in segments.
    uint32_t rowsPerUnit = 0;
    uint32_t segmentsPerRow = 1;
    uint32_t unitCount = 0;
};

inline UnitLayout makeUnitLayout(Format f, int width, int height, int maxDatagram) {
    UnitLayout layout;
    const uint32_t blockSize = blockBytes(f);
    layout.rowBytes = blocksAcross(width) * blockSize;
    layout.rows = blocksAcross(height);
    layout.maxPayload = (static_cast<uint32_t>(maxDatagram) - sizeof(PacketHeader)) / blockSize * blockSize;
    if (layout.rowBytes <= layout.maxPayload) {
        layout.rowsPerUnit = layout.maxPayload / layout.rowBytes;
        layout.unitCount = (layout.rows + layout.rowsPerUnit - 1) / layout.rowsPerUnit;
    } else {
        layout.segmentsPerRow = (layout.rowBytes + layout.maxPayload - 1) / layout.maxPayload;
        layout.unitCount = layout.rows * layout.segmentsPerRow;
    }
    return layout;
}

inline void unitRange(const UnitLayout &layout, uint32_t unit, uint32_t &offset, uint32_t &length) {
    if (layout.rowsPerUnit > 0) {
        const uint32_t row = unit * layout.rowsPerUnit;
        offset = row * layout.rowBytes;
        length = std::min(layout.rowsPerUnit, layout.rows - row) * layout.rowBytes;
    } else {
        const uint32_t row = unit / layout.segmentsPerRow;
        const uint32_t segment = unit % layout.segmentsPerRow;
        offset = row * layout.rowBytes + segment * layout.maxPayload;
        length = std::min(layout.maxPayload, layout.rowBytes - segment * layout.maxPayload);
    }
}

} // namespace nodestream

#endif // NODESTREAMPROTOCOL_H
