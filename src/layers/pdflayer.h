#ifndef PDFLAYER_H
#define PDFLAYER_H

#include <layers/baselayer.h>
#include <sgct/opengl.h>
#include <poppler-document.h>
#include <poppler-image.h>

class PdfLayer : public BaseLayer {
public:
    struct PdfData {
        std::string filepath = "";
        poppler::document* document = nullptr;
        int page = 0;
        double dpi = 72.0;
        poppler::image img;
        std::unique_ptr<std::thread> trd;
        std::atomic_bool threadRunning = false;
        std::atomic_bool pageDone = false;
        std::atomic_bool uploadDone = false;
        std::atomic_bool threadDone = false;
    };

    PdfLayer();
    ~PdfLayer();

    void initialize();
    void update();
    bool ready();

    void start();
    void stop();

private:
    PdfData m_pdfData;

    bool loadDocument(std::string filepath);
    void handleAsyncPageRender();
    void loadPageAsTexture(GLuint TextureID, unsigned int width, unsigned int height, poppler::image::format_enum format, const char* data);
    void createPageAsTexture(unsigned int& id, int width, int height, poppler::image::format_enum format, const char* data = nullptr);
};

#endif // PDFLAYER_H