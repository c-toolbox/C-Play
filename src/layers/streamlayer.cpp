/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "streamlayer.h"

StreamLayer::StreamLayer(gl_adress_func_v1 opa,
    bool allowDirectRendering,
    bool loggingOn,
    std::string logLevel,
    MpvLayer::onFileLoadedCallback flc) : VideoLayer(opa, allowDirectRendering, loggingOn, logLevel, flc) {
    m_data.isStream = true;
    setType(BaseLayer::LayerType::STREAM);
}

StreamLayer::~StreamLayer() {
}

void StreamLayer::initialize() {
    VideoLayer::initialize();
}

bool StreamLayer::ready() const {
    return !m_data.loadedFile.empty();
}
