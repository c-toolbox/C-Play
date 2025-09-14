/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "textlayer.h"
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
    setGridMode(BaseLayer::GridMode::Plane);
    setStereoMode(BaseLayer::StereoMode::No_2D);
    renderData.width = 3840;
    renderData.height = 2160;
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
    std::vector<std::string> lines = split(std::move(m_data.text), '\n');

    const glm::vec2 res = glm::vec2(m_data.fboWidth, m_data.fboHeight);
    const glm::mat4 orthoMatrix = glm::ortho(0.f, res.x, 0.f, res.y);

    const float offsetX = res.x / 2.f - res.x / 7.f;
    const float s1 = res.y / 8.f;
    const float offsetY = res.y / 2.f - s1;
    const sgct::vec4 color = sgct::vec4{ 0.f, 0.f, 1.f, 1.f };

    const unsigned int fontSize1 = static_cast<unsigned int>(s1);
    sgct::text::Font* font = sgct::text::FontManager::instance().font("SGCTFont", fontSize1);

    sgct::text::Alignment mode = sgct::text::Alignment::TopLeft;

    const float h = font->height() * 1.59f;

    glBindFramebuffer(GL_FRAMEBUFFER, m_data.fboId);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(font->vao());

    for (size_t i = 0; i < lines.size(); i++) {
        glm::vec3 offset(offsetX, offsetY - h * i, 0.f);

        if (mode == sgct::text::Alignment::TopCenter) {
            offset.x -= getLineWidth(*font, lines[i]) / 2.f;
        }
        else if (mode == sgct::text::Alignment::TopRight) {
            offset.x -= getLineWidth(*font, lines[i]);
        }

        for (const char c : lines[i]) {
            const sgct::text::Font::FontFaceData& ffd = font->fontFaceData(c);

            glBindTexture(GL_TEXTURE_2D, ffd.texId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

            const glm::mat4 trans = glm::translate(
                orthoMatrix,
                glm::vec3(offset.x + ffd.pos.x, offset.y + ffd.pos.y, offset.z)
            );
            glm::mat4 scale = glm::scale(trans, glm::vec3(ffd.size.x, ffd.size.y, 1.f));
            sgct::mat4 s;
            std::memcpy(&s, glm::value_ptr(scale), sizeof(sgct::mat4));

            sgct::text::FontManager::instance().bindShader(s, color, 0);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            offset += glm::vec3(ffd.distToNextChar, 0.f, 0.f);
        }
    }

    glDisable(GL_BLEND);
    glBindVertexArray(0);
    sgct::ShaderProgram::unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
}

bool TextLayer::ready() const {
    // Overwrite in derived class
    return hasText();
}

bool TextLayer::hasText() const {
    return text() != "";
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