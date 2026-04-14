/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef QRCODEREADER_H
#define QRCODEREADER_H

#include <string>
#include <vector>

class QRCodeReader {
public:
    QRCodeReader();
    ~QRCodeReader();

    // Scan image data for QR codes. Returns decoded text strings.
    // pixelData: pointer to image pixel data
    // width, height: image dimensions
    // GLformat: GL_BGRA or GL_RGBA
    std::vector<std::string> scan(unsigned char* pixelData, unsigned int width, unsigned int height, int GLformat);

private:
    struct Impl;
    Impl* m_impl = nullptr;
};

#endif // QRCODEREADER_H
