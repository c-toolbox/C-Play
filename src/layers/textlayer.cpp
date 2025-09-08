/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "textlayer.h"

TextLayer::TextLayer() {
    setType(BaseLayer::LayerType::TEXT);
}

TextLayer::~TextLayer() {
}

void TextLayer::initialize() {
    // Overwrite in derived class
}

void TextLayer::initializeGL() {
    // Overwrite in derived class
}

void TextLayer::update(bool) {
    // Overwrite in derived class
}

void TextLayer::updateFrame() {
    // Overwrite in derived class
}

bool TextLayer::renderingIsOn() const {
    // Overwrite in derived class
    return false;
}

bool TextLayer::ready() const {
    // Overwrite in derived class
    return false;
}