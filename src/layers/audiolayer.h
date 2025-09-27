/*
 * SPDX-FileCopyrightText:
 * 2024-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef AUDIOLAYER_H
#define AUDIOLAYER_H

#include <layers/mpvlayer.h>

class AudioLayer : public MpvLayer {
public:
    AudioLayer(gl_adress_func_v1 opa,
        bool allowDirectRendering = false,
        bool loggingOn = false,
        std::string logLevel = "info");
    ~AudioLayer();

    bool ready() const;
    bool existOnMasterOnly() const;
};

#endif // AUDIOLAYER_H