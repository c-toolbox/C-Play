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
#include <string>

class TextLayer : public BaseLayer {
public:
    struct textData {
        bool initializedGL = false;
        int fboWidth = 0;
        int fboHeight = 0;
        bool fboCreated = false;
        unsigned int fboId = 0;
        std::string text = "";
    };

    TextLayer();
    ~TextLayer();

    void initialize();
    void initializeGL();

    void update(bool updateRendering = true);
    void updateFrame();
    bool ready() const;

    bool hasText() const;

private:
    void checkNeededFboResize();
    void createFBO(int width, int height);
    void generateTexture(unsigned int& id, int width, int height);

    textData m_data;
};

#endif // TEXTLAYER_H
