#ifndef PDFLAYER_H
#define PDFLAYER_H

#include <layers/baselayer.h>
#include <sgct/opengl.h>
#include <cpp/poppler-document.h>
#include <cpp/poppler-image.h>
#include <map>

class PdfDocumentManager {
public:
    PdfDocumentManager();
    ~PdfDocumentManager();

    static PdfDocumentManager& instance();
    std::shared_ptr<poppler::document> getDocument(std::string filepath);
    void trashDocument(std::string filepath);

private:
    struct PDFDocument {
        int retrievals = 0;
        std::shared_ptr<poppler::document> document = nullptr;
    };

    static PdfDocumentManager* _instance;
    std::map<std::string, PDFDocument> m_documents;
};

class PdfLayer : public BaseLayer {
public:
    struct PdfData {
        std::string filepath = "";
        std::shared_ptr<poppler::document> document = nullptr;
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
    void update(bool updateRendering = true);
    bool ready() const;

private:
    PdfData m_pdfData;

    bool loadDocument(std::string filepath);
    void handleAsyncPageRender();
    void loadPageAsTexture(GLuint TextureID, unsigned int width, unsigned int height, poppler::image::format_enum format, const char* data);
    void createPageAsTexture(unsigned int& id, int width, int height, poppler::image::format_enum format, const char* data = nullptr);
};

#endif // PDFLAYER_H