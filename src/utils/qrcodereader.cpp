/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "qrcodereader.h"
#include <sgct/opengl.h>

#ifdef ZXING_SUPPORT
#include <ZXing/ZXingCpp.h>
#endif

struct QRCodeReader::Impl {
#ifdef ZXING_SUPPORT
    ZXing::ReaderOptions options;
#endif
};

QRCodeReader::QRCodeReader()
    : m_impl(new Impl)
{
#ifdef ZXING_SUPPORT
    m_impl->options = ZXing::ReaderOptions();
    m_impl->options.setFormats(ZXing::BarcodeFormat::QRCode);
    m_impl->options.setTryRotate(false);
    m_impl->options.setTryInvert(false);
    m_impl->options.setTryDownscale(false);
    m_impl->options.setTextMode(ZXing::TextMode::Plain);
#endif
}

QRCodeReader::~QRCodeReader() {
    delete m_impl;
}

std::vector<std::string> QRCodeReader::scan(unsigned char* pixelData, unsigned int width, unsigned int height, int GLformat) {
    std::vector<std::string> results;

    if (GLformat != GL_BGRA && GLformat != GL_RGBA) {
        return results;
    }

#ifdef ZXING_SUPPORT
    ZXing::ImageFormat imageFormat = (GLformat == GL_BGRA) ? ZXing::ImageFormat::BGRA : ZXing::ImageFormat::RGBA;

    auto image = ZXing::ImageView(pixelData, width, height, imageFormat);
    auto codes = ZXing::ReadBarcodes(image, m_impl->options);

    for (const auto& b : codes) {
        if (!b.text().empty()) {
            results.push_back(b.text());
        }
    }
#endif

    return results;
}
