#include "pdflayer.h"
#include <presentationsettings.h>
#include <sgct/sgct.h>
#include <fmt/core.h>
#include <poppler-page.h>
#include <poppler-page-renderer.h>

auto loadPageAsync = [](PdfLayer::PdfData& data) {
    data.threadRunning = true;

    std::unique_ptr<poppler::page> p(data.document->create_page(data.page - 1));
    if (!p.get()) {
        sgct::Log::Error(fmt::format("PDF creation of page {} in {} failed.", data.page, data.filepath));
    }

    poppler::page_renderer pr;
    pr.set_render_hint(poppler::page_renderer::antialiasing, true);
    pr.set_render_hint(poppler::page_renderer::text_antialiasing, true);

    data.img = pr.render_page(p.get(), data.dpi, data.dpi);
    if (!data.img.is_valid()) {
        sgct::Log::Error(fmt::format("PDF rendering of page {} in {} failed.", data.page, data.filepath));
    }

    data.pageDone = true;
    while (!data.uploadDone) {
    }
    data.img = poppler::image(); // Assigning invalid image
    data.threadDone = true;
};

PdfDocumentManager* PdfDocumentManager::_instance = nullptr;

PdfDocumentManager::PdfDocumentManager() {}

PdfDocumentManager::~PdfDocumentManager() {
    delete _instance;
    _instance = nullptr;
}

PdfDocumentManager& PdfDocumentManager::instance() {
    if (!_instance) {
        _instance = new PdfDocumentManager();
    }
    return *_instance;
}

poppler::document* PdfDocumentManager::getDocument(std::string filepath) {
    auto it = m_documents.find(filepath);

    if (it == m_documents.end()) {
        // Not found, let's load it.
        poppler::document* docPtr = poppler::document::load_from_file(filepath);

        if (docPtr == nullptr) {
            return nullptr;
        }
        else {
            // Loaded OK,let's store and return it
            PDFDocument newDoc;
            newDoc.retrievals = 1;
            newDoc.document = docPtr;
            it = m_documents.insert(std::make_pair(filepath, newDoc)).first;
            return it->second.document;
        }
    }
    else {
        // Found it, let's return it after we have marked another retrieval
        it->second.retrievals += 1;
        return it->second.document;
    }
}

void PdfDocumentManager::trashDocument(std::string filepath) {
    auto it = m_documents.find(filepath);
    if (it != m_documents.end()) {
        it->second.retrievals -= 1;
        if (it->second.retrievals == 0) {
            delete it->second.document;
            m_documents.erase(it);
        }
    }
}

PdfLayer::PdfLayer() {
    setType(BaseLayer::LayerType::PDF);
    setNumPages(1);
    setPage(1);
    m_pdfData.dpi = PresentationSettings::pdfDpi();
    renderData.flipY = true;
}

PdfLayer::~PdfLayer() {
    if (renderData.texId > 0) {
        glDeleteTextures(1, &renderData.texId);
    }
    if (m_pdfData.document != nullptr) {
        PdfDocumentManager::instance().trashDocument(m_pdfData.filepath);
    }
}

void PdfLayer::initialize() {
    loadDocument(filepath());
    m_hasInitialized = true;
}

void PdfLayer::update(bool updateRendering) {
    if(updateRendering || !ready())
        handleAsyncPageRender();

    if (m_pdfData.threadRunning)
        return;

    bool loadDoc = false;
    if (m_pdfData.document == nullptr) {
        loadDoc = true;
    }
    else if (m_pdfData.filepath != filepath()) {
        if (m_pdfData.document != nullptr) {
            PdfDocumentManager::instance().trashDocument(m_pdfData.filepath);
            m_pdfData.document = nullptr;
        }
        loadDoc = true;
    }

    bool loadPage = false;
    if (loadDoc) {
        loadPage = loadDocument(filepath());
    }
    else if(m_pdfData.page != page()){
        loadPage = true;
    }

    if ((updateRendering || !ready()) && loadPage && page() > 0) {
        m_pdfData.page = page();
        sgct::Log::Info(fmt::format("Loading page {} in {} asynchronously.", m_pdfData.page, m_pdfData.filepath));
        m_pdfData.trd = std::make_unique<std::thread>(loadPageAsync, std::ref(m_pdfData));
    }
}

bool PdfLayer::ready() const {
    return m_pdfData.filepath == filepath() && m_pdfData.page == page() && m_pdfData.trd == nullptr;
}

bool PdfLayer::loadDocument(std::string filepath) {
    m_pdfData.filepath = filepath;
    if (!m_pdfData.filepath.empty()) {
        m_pdfData.document = PdfDocumentManager::instance().getDocument(m_pdfData.filepath);
    }

    if (m_pdfData.document == nullptr) {
        sgct::Log::Error(fmt::format("Loading error: PDF {} failed", filepath));
    }
    else if (m_pdfData.document->is_locked()) {
        sgct::Log::Error(fmt::format("Loading error: PDF {} is encrypted.", filepath));
    }
    else { //Success
        setNumPages(m_pdfData.document->pages());
        return true;
    }

    return false;
}

void PdfLayer::handleAsyncPageRender() {
    if (m_pdfData.threadRunning) {
        if (m_pdfData.pageDone && !m_pdfData.uploadDone) {
            if (renderData.texId == 0 || renderData.width != m_pdfData.img.width() || renderData.height != m_pdfData.img.height()) {
                if (renderData.texId > 0)
                    glDeleteTextures(1, &renderData.texId);

                createPageAsTexture(renderData.texId, m_pdfData.img.width(), m_pdfData.img.height(), m_pdfData.img.format(), m_pdfData.img.const_data());
                renderData.width = m_pdfData.img.width();
                renderData.height = m_pdfData.img.height();
            }
            else {
                loadPageAsTexture(renderData.texId, m_pdfData.img.width(), m_pdfData.img.height(), m_pdfData.img.format(), m_pdfData.img.const_data());
            }
            sgct::Log::Info(fmt::format("Page {} in {} loaded with width {} and height {}.", m_pdfData.page, m_pdfData.filepath, renderData.width, renderData.height));
            m_pdfData.uploadDone = true;
        }
        else if (m_pdfData.threadDone) {
            m_pdfData.threadRunning = false;
            m_pdfData.pageDone = false;
            m_pdfData.uploadDone = false;
            m_pdfData.threadDone = false;
            m_pdfData.trd->join();
            m_pdfData.trd = nullptr;
        }
    }
}

void PdfLayer::loadPageAsTexture(GLuint TextureID, unsigned int width, unsigned int height, poppler::image::format_enum format, const char* data) {
    // Bind the texture and PBO
    glBindTexture(GL_TEXTURE_2D, TextureID);

    switch (format) {
    case poppler::image::format_enum::format_argb32:
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data);
        break;
    case poppler::image::format_enum::format_bgr24:
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, data);
        break;
    case poppler::image::format_enum::format_gray8:
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, data);
        break;
    case poppler::image::format_enum::format_mono:
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, data);
        break;
    case poppler::image::format_enum::format_rgb24:
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
        break;
    case poppler::image::format_enum::format_invalid:
    default:
        break;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void PdfLayer::createPageAsTexture(unsigned int& id, int width, int height, poppler::image::format_enum format, const char* data) {
    glGenTextures(1, &id);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBindTexture(GL_TEXTURE_2D, id);

    switch (format) {
    case poppler::image::format_enum::format_argb32:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data);
        break;
    case poppler::image::format_enum::format_bgr24:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data);
        break;
    case poppler::image::format_enum::format_gray8:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, data);
        break;
    case poppler::image::format_enum::format_mono:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, data);
        break;
    case poppler::image::format_enum::format_rgb24:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        break;
    case poppler::image::format_enum::format_invalid:
    default:
        break;
    }

    // Disable mipmaps
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
}
