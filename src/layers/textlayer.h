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

    std::string text() const;
    void setText(std::string t);

    std::string fontName() const;
    void setFont(std::string name);

    int fontSize() const;
    void setFontSize(int s);

    int alignment() const;
    std::string alignmentStr() const;
    void setAlignment(int a);
    void setAlignmentFromStr(const std::string&);

    float colorR() const;
    float colorG() const;
    float colorB() const;
    std::string colorHex() const;
    void setColor(std::string hex, float r, float g, float b);

    void setTextureSize(int w, int h);

    void encodeTypeAlways(std::vector<std::byte>& data);
    void decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos);

    void encodeTypeProperties(std::vector<std::byte>& data);
    void decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos);

private:
    void checkNeededFboResize();
    void createFBO(int width, int height);
    void generateTexture(unsigned int& id, int width, int height);

    bool m_textIsChanged;
    std::string m_text;
    std::string m_fontName;
    std::string m_fontPath;
    int m_fontSize;
    int m_alignment;
    std::string m_colorHex;
    sgct::vec4 m_color;
    textData m_data;
};

#endif // TEXTLAYER_H
