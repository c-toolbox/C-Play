/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "textlayer.h"
#include "application.h"
#include <sgct/opengl.h>
#include <sgct/sgct.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifdef SGCT_HAS_TEXT
#include <freetype/freetype.h>
#include <sgct/font.h>
#include <sgct/fontmanager.h>

static std::vector<std::string> split(std::string str, char delimiter) {
    std::stringstream ss(std::move(str));
    std::string part;

    std::vector<std::string> tmpVec;
    while (getline(ss, part, delimiter)) {
        tmpVec.push_back(part);
    }

    return tmpVec;
}

static float getLineWidth(sgct::text::Font& font, const std::string& line) {
    if (line.empty())
        return 0;

    // figure out width
    float lineWidth = 0.f;
    for (size_t j = 0; j < line.length() - 1; j++) {
        const char c = line.c_str()[j];
        const sgct::text::Font::FontFaceData& ffd = font.fontFaceData(c);
        lineWidth += ffd.distToNextChar;
    }
    // add last char width
    const char c = line.c_str()[line.length() - 1];
    const sgct::text::Font::FontFaceData& ffd = font.fontFaceData(c);
    lineWidth += ffd.size.x;

    return lineWidth;
}
#endif

TextLayer::TextLayer() {
#ifdef SGCT_HAS_TEXT
    setType(BaseLayer::LayerType::TEXT);
#endif
    m_textIsChanged = false;
    m_text = "";
    m_fontName = "";
    m_fontPath = "";
    m_fontSize = 64;
    m_alignment = 1; // Center
    m_colorHex = "#FFFFFF";
    m_color = sgct::vec4{ 1.f, 1.f, 1.f, 1.f };
    renderData.width = 1280;
    renderData.height = 720;
    setGridMode(BaseLayer::GridMode::Plane);
    setStereoMode(BaseLayer::StereoMode::No_2D);
}

TextLayer::~TextLayer() {
    if (m_data.fboCreated) {
        glDeleteFramebuffers(1, &m_data.fboId);
        glDeleteTextures(1, &renderData.texId);
        m_data.fboCreated = false;
    }
}

void TextLayer::initialize() {
    // Overwrite in derived class
}

void TextLayer::initializeGL() {
    checkNeededFboResize();
}

void TextLayer::update(bool updateRendering) {
    if (!m_data.initializedGL) {
        initializeGL();
    }

    if (updateRendering) {
        updateFrame();
    }
}

void TextLayer::updateFrame() {
    checkNeededFboResize();

    // If new text and rendered text is the same, we do not need to update
    if (text() == m_data.text) {
        return;
    }
    m_data.text = text();

#ifdef SGCT_HAS_TEXT
    if (m_fontName.empty()) {
        sgct::Log::Warning("TextLayer: Font name is empty.");
        return;
    }
    sgct::text::Font* font = sgct::text::FontManager::instance().font(m_fontName, m_fontSize);
    if (font == nullptr) {
        // Trying to add the font to the font manager
        if (!m_fontPath.empty()) {
            if (sgct::text::FontManager::instance().addFont(m_fontName, m_fontPath, true)) {
                font = sgct::text::FontManager::instance().font(m_fontName, m_fontSize);
                if (font == nullptr) {
                    sgct::Log::Warning(std::format("TextLayer: Font {} with path {} could not be added.", m_fontName, m_fontPath));
                    return;
                }
            }
            else {
                sgct::Log::Warning(std::format("TextLayer: Font {} with path {} was not found.", m_fontName, m_fontPath));
                return;
            }
        }
        else {
            sgct::Log::Warning(std::format("TextLayer: Font path is empty for font {}", m_fontName));
            return;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_data.fboId);

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glViewport(0, 0, m_data.fboWidth, m_data.fboHeight);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(font->vao());

    std::vector<std::string> lines = split(std::move(m_data.text), '\n');
    const glm::vec2 res = glm::vec2(m_data.fboWidth, m_data.fboHeight);
    const glm::mat4 orthoMatrix = glm::ortho(0.f, res.x, 0.f, res.y);
    const float h = font->height() * 1.59f;
    const float offsetX = 0;
    const float offsetY = h * lines.size();
    const float marginX = res.x / 7.f;
    sgct::text::Alignment alignment = static_cast<sgct::text::Alignment>(m_alignment);
    for (size_t i = 0; i < lines.size(); i++) {
        glm::vec3 offset(offsetX, offsetY - h * i, 0.f);

        if (alignment == sgct::text::Alignment::TopCenter) {
            offset.x = (res.x / 2) - (getLineWidth(*font, lines[i]) / 2.f);
        }
        else if (alignment == sgct::text::Alignment::TopRight) {
            offset.x = res.x - getLineWidth(*font, lines[i]);
        }
        else { //TopLeft
            offset.x = marginX;
        }

        for (const char c : lines[i]) {
            const sgct::text::Font::FontFaceData& ffd = font->fontFaceData(c);

            glBindTexture(GL_TEXTURE_2D, ffd.texId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

            const glm::mat4 trans = glm::translate(
                orthoMatrix,
                glm::vec3(offset.x + ffd.pos.x, offset.y + ffd.pos.y, offset.z)
            );
            glm::mat4 scale = glm::scale(trans, glm::vec3(ffd.size.x, ffd.size.y, 1.f));
            sgct::mat4 s;
            std::memcpy(&s, glm::value_ptr(scale), sizeof(sgct::mat4));

            sgct::text::FontManager::instance().bindShader(s, m_color, 0);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            offset += glm::vec3(ffd.distToNextChar, 0.f, 0.f);
        }
    }

    glDisable(GL_BLEND);
    glBindVertexArray(0);
    sgct::ShaderProgram::unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
#endif
}

bool TextLayer::ready() const {
    // Overwrite in derived class
    return hasText();
}

bool TextLayer::hasText() const {
    return text() != "";
}

std::string TextLayer::text() const {
    return m_text;
}

void TextLayer::setText(std::string t) {
    m_text = t;
    m_textIsChanged = true;
}

std::string TextLayer::fontName() const {
    return m_fontName;
}

void TextLayer::setFont(std::string name) {
    if (Application::instance().getFontPath(name, m_fontPath)) {
        m_fontName = name;
        setNeedSync();
    }
}

int TextLayer::fontSize() const {
    return m_fontSize;
}

void TextLayer::setFontSize(int s) {
    m_fontSize = s;
    setNeedSync();
}

int TextLayer::alignment() const {
    return m_alignment;
}

std::string TextLayer::alignmentStr() const {
#ifdef SGCT_HAS_TEXT
    switch (static_cast<sgct::text::Alignment>(m_alignment)) {
    case sgct::text::Alignment::TopLeft:
        return "left";
    case sgct::text::Alignment::TopRight:
        return "right";
    case sgct::text::Alignment::TopCenter:
    default:
        return "center";
    }
#else
    return "center";
#endif
}

void TextLayer::setAlignment(int a) {
    m_alignment = a;
    setNeedSync();
}

void TextLayer::setAlignmentFromStr(const std::string& str) {
#ifdef SGCT_HAS_TEXT
    std::string strToLower = str;
    std::transform(strToLower.begin(), strToLower.end(), strToLower.begin(),
        [](unsigned char c) { return std::tolower(c); });
    if (strToLower == "left") {
        setAlignment(static_cast<int>(sgct::text::Alignment::TopLeft));
    }
    else if(strToLower == "right"){
        setAlignment(static_cast<int>(sgct::text::Alignment::TopRight));
    }
    else { // center
        setAlignment(static_cast<int>(sgct::text::Alignment::TopCenter));
    }
#else
    setAlignment(1);
#endif
}

float TextLayer::colorR() const {
    return m_color.x;
}

float TextLayer::colorG() const {
    return m_color.y;
}

float TextLayer::colorB() const {
    return m_color.z;
}

std::string TextLayer::colorHex() const {
    return m_colorHex;
}

void TextLayer::setColor(std::string hex, float r, float g, float b) {
    m_colorHex = hex;
    m_color.x = r;
    m_color.y = g;
    m_color.z = b;
    setNeedSync();
}

void TextLayer::setTextureSize(int w, int h) {
    renderData.width = w;
    renderData.height = h;
    setNeedSync();
}

void TextLayer::encodeTypeAlways(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_textIsChanged);
    if (m_textIsChanged) {
        sgct::serializeObject(data, m_text);
        m_textIsChanged = false;
    }
}

void TextLayer::decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_textIsChanged);
    if (m_textIsChanged) {
        sgct::deserializeObject(data, pos, m_text);
        m_textIsChanged = false;
    }
}

void TextLayer::encodeTypeProperties(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_fontName);
    sgct::serializeObject(data, m_fontPath);
    sgct::serializeObject(data, m_fontSize);
    sgct::serializeObject(data, m_alignment);
    sgct::serializeObject(data, m_color.x);
    sgct::serializeObject(data, m_color.y);
    sgct::serializeObject(data, m_color.z);
    sgct::serializeObject(data, renderData.width);
    sgct::serializeObject(data, renderData.height);
}

void TextLayer::decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_fontName);
    sgct::deserializeObject(data, pos, m_fontPath);
    sgct::deserializeObject(data, pos, m_fontSize);
    sgct::deserializeObject(data, pos, m_alignment);
    sgct::deserializeObject(data, pos, m_color.x);
    sgct::deserializeObject(data, pos, m_color.y);
    sgct::deserializeObject(data, pos, m_color.z);
    sgct::deserializeObject(data, pos, renderData.width);
    sgct::deserializeObject(data, pos, renderData.height);
}

void TextLayer::checkNeededFboResize() {
    if (m_data.fboWidth == renderData.width && m_data.fboHeight == renderData.height)
        return;

    int maxTexSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    if (renderData.width <= 0 || renderData.height <= 0 || renderData.width > maxTexSize || renderData.height > maxTexSize)
        return;

    sgct::Log::Info(std::format("New FBO width:{} and height:{}", renderData.width, renderData.height));

    createFBO(renderData.width, renderData.height);
}

void TextLayer::createFBO(int width, int height) {
    if (m_data.fboCreated) {
        glDeleteFramebuffers(1, &m_data.fboId);
        glDeleteTextures(1, &renderData.texId);
    }

    m_data.fboWidth = width;
    m_data.fboHeight = height;

    glGenFramebuffers(1, &m_data.fboId);
    glBindFramebuffer(GL_FRAMEBUFFER, m_data.fboId);

    generateTexture(renderData.texId, width, height);

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        renderData.texId,
        0);

    m_data.fboCreated = true;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void TextLayer::generateTexture(unsigned int& id, int width, int height) {
    glGenTextures(1, &id);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBindTexture(GL_TEXTURE_2D, id);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);

    // Disable mipmaps
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}