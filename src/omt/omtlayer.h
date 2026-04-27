/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef OMTLAYER_H
#define OMTLAYER_H

#include <layers/baselayer.h>
#include <sgct/opengl.h>
#include <libomt.h>

class OmtFinder {
public:
    OmtFinder();
    ~OmtFinder();
    static OmtFinder& instance();

    std::vector<std::string> getSendersList();
    bool senderExists(const std::string& senderName);

    std::string getOMTVersionString();

private:
    static OmtFinder* _instance;
};

class OmtLayer : public BaseLayer {
public:
    OmtLayer();
    ~OmtLayer();

    void initialize();
    void update(bool updateRendering = true);
    void updateFrame();
    bool ready() const;
    bool hasTexture() const override;

private:
    void GenerateTexture(unsigned int& id, int width, int height);

    omt_receive_t* m_receiver = nullptr;
    bool m_isReady = false;
};

#endif // OMTLAYER_H
