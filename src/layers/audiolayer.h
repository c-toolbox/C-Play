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
};

#endif // AUDIOLAYER_H