/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SPOUTLAYER_H
#define SPOUTLAYER_H

#include <layers/baselayer.h>
#include <SpoutLibrary/SpoutLibrary.h>

class SpoutFinder {
public:
    SpoutFinder();
    ~SpoutFinder();
    static SpoutFinder& instance();

    int senderCount();
    std::vector<std::string> getSendersList();
    bool senderExists(std::string senderName);

    std::string getSpoutVersionString();

private:
    SPOUTLIBRARY* m_spoutlibrary;
    static SpoutFinder* _instance;
};

class SpoutLayer : public BaseLayer {
public:
    SpoutLayer();
    ~SpoutLayer();

    void initialize();
    void update(bool updateRendering = true);
    bool ready() const;

private:
    void GenerateTexture(unsigned int& id, int width, int height);

    SPOUTLIBRARY* m_receiver;
};

#endif // SPOUTLAYER_H