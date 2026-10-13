/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TEXTURESUBDIVISION_H
#define TEXTURESUBDIVISION_H

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class BaseLayer;
class DivideTextureHandler;
class QRCommandProcessor;
class QROperationConfig;
class QROperationHandler;
struct QRCommand;

// Layer operations for layers that receive a live texture (NDI, OMT, Stream, DirectShow,
// Capture, Spout, WebRTC): ImPres mode (QR control frames driving TextureLayer sublayers,
// see QROperationHandler) or division mode (the texture split into a grid of sublayers,
// see DivideTextureHandler).
//
// A layer enables it through BaseLayer::enableTextureSubdivision(); BaseLayer's division/QR
// virtuals then delegate here. Per frame the layer either scans the CPU pixels before upload
// (isControlFrame) or the uploaded texture (filterControlFrame), then calls updateSubLayers()
// with the texture it shows. The state is synced in the layer's type sections through
// encode/decodeAlways() and encode/decodeProperties().
class TextureSubdivision {
public:
    enum Mode { None = 0, ImPres = 1, Division = 2 };

    explicit TextureSubdivision(BaseLayer* owner);
    ~TextureSubdivision();

    bool qrEnabled() const;
    void setQrEnabled(bool enabled);

    int mode() const;
    void setMode(int mode);

    // Grid index: 0=1x1, 1=1x2, 2=2x1, 3=2x2, 4=2x3, 5=3x2, 6=3x3
    int grid() const;
    void setGrid(int grid);
    static void gridToColsRows(int grid, int& cols, int& rows);

    // Merge layout of the division cells, see BaseLayer::setTextureDivisionMerge().
    // 0x0 = custom. A grid change resets a layout that no longer fits to the grid itself.
    int mergeColumns() const;
    int mergeRows() const;
    bool setMerge(int cols, int rows);
    // A layout fits when it holds all cells, filled row by row, with none of its rows or
    // columns left empty: 3x3 also fits 9x1, 1x9, 5x2 or 6x2.
    static bool isValidMerge(int cols, int rows, int cellCount);
    // The layouts fitting cellCount, by increasing column count.
    static std::vector<std::pair<int, int>> validMerges(int cellCount);
    // Re-aligns the cells to the merge layout (no-op for a custom layout).
    void applyMerge();

    bool loadQROperationConfig(const std::string& filePath);
    const QROperationConfig* qrOperationConfig() const;
    std::string activePlaneName() const;

    bool hasSubLayers() const;
    std::vector<std::shared_ptr<BaseLayer>>& subLayers() const;

    // CPU path, called before uploading a new frame. Returns true for a QR control frame,
    // which must not be uploaded. Queued QR commands run against the texture last passed to
    // updateSubLayers(), i.e. the last clean frame on screen.
    bool isControlFrame(unsigned char* pixels, unsigned int width, unsigned int height,
                        int glFormat, int rowStride = 0, int pixelStride = 0);

    // GPU path for layers without CPU pixels, called once texId holds a new frame. Returns the
    // texture to show: texId, or a copy of the last clean frame while texId holds a QR control
    // frame. Reads the texture back, so only when ImPres mode is on.
    unsigned int filterControlFrame(unsigned int texId, int width, int height);

    // Called with the texture the layer shows: feeds the active QR plane or the division cells.
    void updateSubLayers(unsigned int texId, int width, int height);

    // Releases the OpenGL resources. Requires a current context.
    void releaseGL();

    void encodeAlways(std::vector<std::byte>& data) const;
    void decodeAlways(const std::vector<std::byte>& data, unsigned int& pos);
    void encodeProperties(std::vector<std::byte>& data) const;
    void decodeProperties(const std::vector<std::byte>& data, unsigned int& pos);

    // What a layer without layer operations encodes, matching decodeAlways()/decodeProperties().
    static void encodeDefaults(std::vector<std::byte>& data, bool properties);

private:
    void applyDecoded();
    void applyDivision();
    void onQRCommand(const QRCommand& command);
    void copyToBackup(unsigned int texId, int width, int height);

    BaseLayer* m_owner;
    std::unique_ptr<QRCommandProcessor> m_qrProcessor;
    std::unique_ptr<QROperationHandler> m_qrOpHandler;
    std::unique_ptr<DivideTextureHandler> m_divideTexHandler;
    int m_mode = None;
    int m_grid = 0;
    // Starts as the 1x1 grid itself, so a new division is merged like the source.
    int m_mergeCols = 1;
    int m_mergeRows = 1;

    // QR detection is applied on the render thread (it may release sublayers).
    bool m_qrEnabledDecoded = false;
    bool m_qrDecodePending = false;

    // Texture queued QR commands run against.
    unsigned int m_liveTex = 0;
    int m_liveWidth = 0;
    int m_liveHeight = 0;

    // GPU path: readback buffer for scanning and the last clean frame.
    std::vector<unsigned char> m_readback;
    unsigned int m_backupTex = 0;
    int m_backupWidth = 0;
    int m_backupHeight = 0;
    int m_backupFormat = 0;
};

#endif // TEXTURESUBDIVISION_H
