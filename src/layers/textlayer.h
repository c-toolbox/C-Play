/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TEXTLAYER_H
#define TEXTLAYER_H

#include <layers/baselayer.h>
#include <sgct/sgct.h>

class TextLayer : public BaseLayer {
public:

    TextLayer();
    ~TextLayer();

    void initialize();
    void initializeGL();

    void update(bool updateRendering = true);
    void updateFrame();
    bool renderingIsOn() const;
    bool ready() const;
};

#endif // TEXTLAYER_H
