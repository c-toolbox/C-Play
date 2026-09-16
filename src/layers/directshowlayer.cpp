/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "directshowlayer.h"
#include <sgct/opengl.h>
#include <sgct/sgct.h>
#include <format>

std::mutex DirectShowLayer::s_pendingTexDeleteMutex;
std::vector<unsigned int> DirectShowLayer::s_pendingTexToDelete;

// The classic DirectShow Sample Grabber symbols are not declared by this machine's
// Windows SDK headers - they are provided by strmiids.lib (linked in CMake).
extern "C" {
extern const GUID CLSID_SampleGrabber;
extern const GUID IID_ISampleGrabber;
extern const GUID IID_ISampleGrabberCB;
}

namespace {

// Classic DirectShow GUIDs that this machine's Windows SDK does not declare.
const GUID kPinCategoryVideo{0x73604c70, 0x829a, 0x11ce, {0xbf, 0xdd, 0x00, 0xaa, 0x00, 0x66, 0xf3, 0x19}};
const GUID kMediaSubtypeBGRA{0xe436eb7f, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
// 'I420' - not declared by this machine's SDK headers (lives in wmcodecdsp.h, which dshow.h does not include).
const GUID kMediaSubtypeI420{0x30323449, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
// 'YV12' and 'UYVY' - also not declared by this machine's SDK headers.
const GUID kMediaSubtypeYV12{0x32315659, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID kMediaSubtypeUYVY{0x59565955, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID kClsidFileSourceFilter{0xd3588ab0, 0x0781, 0x11ce, {0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID kClsidFileSourceAsync{0xe436ebb5, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID kIidIMediaFile{0x568aab5a, 0xc900, 0x11d3, {0xae, 0x21, 0x00, 0xa0, 0x24, 0x65, 0x7e, 0x0e}};
// Video capture device category CLSID (uuids.h: CLSID_VideoInputDeviceCategory) for
// CreateClassEnumerator(). Not the CATID_* that filters register under - that one
// makes enumeration return S_FALSE.
const GUID kCatVideoInput{0x860bb310, 0x5d01, 0x11d0, {0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86}};

#pragma warning(push)
#pragma warning(disable : 5204) // COM interfaces have no destructor by design
// IMediaFile is not declared by this SDK - classic layout.
struct IMediaFile : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetFileType(LPOLESTR* pFileType) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetURL(LPCWSTR url, AM_MEDIA_TYPE* pmt) = 0;
};
#pragma warning(pop)

// Frees an AM_MEDIA_TYPE allocated by the DirectShow runtime.
void deleteMediaType(AM_MEDIA_TYPE* mt) {
    if (!mt)
        return;
    if (mt->pbFormat)
        free(mt->pbFormat);
    CoTaskMemFree(mt);
}

// How long a running graph may stay frameless before we give up on the file.
constexpr std::chrono::seconds kStallTimeout{10};

// Convert a UTF-8 path to wide characters for the Windows/DirectShow APIs.
std::wstring toWideString(const std::string& utf8) {
    if (utf8.empty())
        return {};
    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (length <= 0)
        return {};
    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
    return wide;
}

// DirectShow RGB32 samples are stored in GDI order: B,G,R,A per pixel. Cameras on
// this machine negotiate YUY2/NV12/I420 (BT.601), which SampleCallback converts.
PixelFormat samplePixelType(const AM_MEDIA_TYPE* mt) {
    if (!mt || !IsEqualGUID(mt->majortype, MEDIATYPE_Video))
        return PixelFormat::Unknown;
    if (IsEqualGUID(mt->subtype, MEDIASUBTYPE_RGB24))
        return PixelFormat::RGB24;
    if (IsEqualGUID(mt->subtype, MEDIASUBTYPE_RGB32) || IsEqualGUID(mt->subtype, kMediaSubtypeBGRA))
        return PixelFormat::RGB32;
    if (IsEqualGUID(mt->subtype, MEDIASUBTYPE_YUY2))
        return PixelFormat::YUY2;
    if (IsEqualGUID(mt->subtype, MEDIASUBTYPE_NV12))
        return PixelFormat::NV12;
    if (IsEqualGUID(mt->subtype, kMediaSubtypeI420))
        return PixelFormat::I420;
    if (IsEqualGUID(mt->subtype, kMediaSubtypeYV12))
        return PixelFormat::YV12;
    if (IsEqualGUID(mt->subtype, kMediaSubtypeUYVY))
        return PixelFormat::UYVY;
    return PixelFormat::Unknown;
}

bool videoInfoSize(const AM_MEDIA_TYPE* mt, int& width, int& height) {
    if (mt && mt->formattype == FORMAT_VideoInfo && mt->pbFormat
        && mt->cbFormat >= static_cast<UINT>(sizeof(VIDEOINFOHEADER))) {
        const VIDEOINFOHEADER* vih = reinterpret_cast<const VIDEOINFOHEADER*>(mt->pbFormat);
        width = static_cast<int>(vih->bmiHeader.biWidth);
        height = static_cast<int>(vih->bmiHeader.biHeight);
        if (height < 0)
            height = -height; // bottom-up bitmaps report negative heights
        return width > 0 && height > 0;
    }
    return false;
}

// BT.601 full-range YCbCr -> RGB (fixed point x1024), written as RGBA.
void ycbcrToRgba(int yy, int cb, int cr, unsigned char* rgba) {
    const int r = yy + ((cr * 1402 + 512) >> 10);
    const int g = yy - ((cb * 344 + 512) >> 10) - ((cr * 714 + 512) >> 10);
    const int b = yy + ((cb * 1772 + 512) >> 10);
    rgba[0] = static_cast<unsigned char>(r < 0 ? 0 : (r > 255 ? 255 : r));
    rgba[1] = static_cast<unsigned char>(g < 0 ? 0 : (g > 255 ? 255 : g));
    rgba[2] = static_cast<unsigned char>(b < 0 ? 0 : (b > 255 ? 255 : b));
    rgba[3] = 255;
}

// Finds a pin of a filter with the given direction. Returns an added reference
// or nullptr.
IPin* findPin(IBaseFilter* filter, PIN_DIRECTION dir) {
    IPin* result = nullptr;
    IEnumPins* pEnum = nullptr;
    if (!filter || FAILED(filter->EnumPins(&pEnum)) || !pEnum)
        return nullptr;

    while (true) {
        IPin* pPin = nullptr;
        ULONG fetched = 0;
        HRESULT hrNext = pEnum->Next(1, &pPin, &fetched);
        if (FAILED(hrNext) || fetched != 1 || !pPin)
            break;

        PIN_DIRECTION pinDir = PINDIR_INPUT;
        const bool keep = SUCCEEDED(pPin->QueryDirection(&pinDir)) && pinDir == dir;
        if (!keep)
            pPin->Release();
        else
            result = pPin; // keep the reference from Next()

        if (result)
            break;
    }

    pEnum->Release();
    return result;
}

// Creates a capture filter for the device with the given friendly name from the video
// input category. Returns an added reference or nullptr when not found.
IBaseFilter* createCaptureFilter(const std::wstring& friendlyName) {
    IBaseFilter* result = nullptr;

    ICreateDevEnum* pSystemDevices = nullptr;
    if (FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                IID_ICreateDevEnum, reinterpret_cast<void**>(&pSystemDevices))) || !pSystemDevices) {
        return nullptr;
    }

    // This SDK's ICreateDevEnum enumerates monikers (not filters) - bind each one to
    // its property bag to read the device's friendly name.
    IEnumMoniker* pCategoryDevices = nullptr;
    const HRESULT hrEnum = pSystemDevices->CreateClassEnumerator(kCatVideoInput, &pCategoryDevices, 0);
    pSystemDevices->Release();
    if (FAILED(hrEnum) || !pCategoryDevices)
        return nullptr;

    IMoniker* pMoniker = nullptr;
    // Compare against S_OK (not SUCCEEDED): on S_FALSE Next() leaves the pointer
    // untouched, so a stale/dangling moniker would be processed once more.
    while (pCategoryDevices->Next(1, &pMoniker, nullptr) == S_OK && pMoniker) {
        IPropertyBag* pPropBag = nullptr;
        if (SUCCEEDED(pMoniker->BindToStorage(nullptr, nullptr, IID_IPropertyBag,
                                              reinterpret_cast<void**>(&pPropBag))) && pPropBag) {
            VARIANT varName{};
            VariantInit(&varName);
            bool match = false;
            if (SUCCEEDED(pPropBag->Read(L"FriendlyName", &varName, nullptr))
                && varName.vt == VT_BSTR && varName.bstrVal) {
                match = wcscmp(varName.bstrVal, friendlyName.c_str()) == 0;
            }
            VariantClear(&varName);
            pPropBag->Release();

            if (match) {
                IBaseFilter* pFilter = nullptr;
                if (SUCCEEDED(pMoniker->BindToObject(nullptr, nullptr, IID_IBaseFilter,
                                                     reinterpret_cast<void**>(&pFilter))) && pFilter) {
                    result = pFilter; // keep the reference from BindToObject()
                }
            }
        }
        pMoniker->Release();
    }

    pCategoryDevices->Release();
    return result;
}

} // namespace

DirectShowLayer::DirectShowLayer() {
    setType(BaseLayer::LayerType::DIRECTSHOW);
#ifdef _WIN32
    m_handoff = std::make_shared<FrameHandoff>();
#endif
}

DirectShowLayer::~DirectShowLayer() {
    cleanup();
}

void DirectShowLayer::cleanup() {
#ifdef _WIN32
    {
        std::lock_guard<std::mutex> lock(m_graphMutex);
        releaseGraph();
    }
#endif
    if (renderData.texId > 0) {
        // No guarantee of a current OpenGL context here - defer the deletion
        // to the render thread, same pattern as ImageLayer.
        std::lock_guard<std::mutex> lock(s_pendingTexDeleteMutex);
        s_pendingTexToDelete.push_back(renderData.texId);
        renderData.texId = 0;
    }
    renderData.width = 0;
    renderData.height = 0;
}

void DirectShowLayer::initialize() {
    m_hasInitialized = true;
}

void DirectShowLayer::setCaptureDevices(const std::string& videoDevice, const std::string& audioDevice) {
    m_captureVideoDevice = videoDevice;
    m_captureAudioDevice = audioDevice;
}

bool DirectShowLayer::ready() const {
    return renderData.texId > 0;
}

bool DirectShowLayer::hasTexture() const {
    return renderData.texId > 0;
}

void DirectShowLayer::update(bool updateRendering) {
#ifdef _WIN32
    ensureGraph();
#endif
    if (updateRendering)
        updateFrame();
}

void DirectShowLayer::updateFrame() {
#ifdef _WIN32
    processPendingGLCleanup();

    // Also handles a file change made through setFilePath() after we became
    // ready - the render loop only calls update() while !ready().
    ensureGraph();

    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;
    if (consumeNewFrame(pixels, width, height))
        uploadFrame(pixels, width, height);
#endif
}

bool DirectShowLayer::consumeNewFrame(std::vector<unsigned char>& pixels, int& width, int& height) {
    if (!m_handoff)
        return false;

    std::lock_guard<std::mutex> lock(m_handoff->mutex);
    if (m_handoff->frameCount == m_consumedFrameCount)
        return false;

    pixels = std::move(m_handoff->pixels);
    width = m_handoff->width;
    height = m_handoff->height;
    m_consumedFrameCount = m_handoff->frameCount;
    return true;
}

void DirectShowLayer::uploadFrame(const std::vector<unsigned char>& pixels, int width, int height) {
#ifdef _WIN32
    if (width <= 0 || height <= 0 || pixels.size() < static_cast<size_t>(width) * height * 4)
        return;

    const bool firstFrame = renderData.texId == 0;
    const bool resized = !firstFrame && (renderData.width != width || renderData.height != height);

    if (firstFrame || resized) {
        if (renderData.texId > 0)
            glDeleteTextures(1, &renderData.texId); // render thread - safe directly

        glGenTextures(1, &renderData.texId);
        glBindTexture(GL_TEXTURE_2D, renderData.texId);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        // Disable mipmaps (matches SpoutLayer)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        renderData.width = width;
        renderData.height = height;

        if (firstFrame) {
            // Top-down pixel data - same convention as ImageLayer.
            setFlipY(true);
            sgct::Log::Info(std::format("DirectShowLayer: first frame uploaded ({}x{})\n", width, height));
        }
    } else {
        glBindTexture(GL_TEXTURE_2D, renderData.texId);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                        GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    }
#endif
}

void DirectShowLayer::processPendingGLCleanup() {
#ifdef _WIN32
    std::vector<unsigned int> toDelete;
    {
        std::lock_guard<std::mutex> lock(s_pendingTexDeleteMutex);
        toDelete.swap(s_pendingTexToDelete);
    }
    if (!toDelete.empty())
        glDeleteTextures(static_cast<GLsizei>(toDelete.size()), toDelete.data());
#endif
}

#ifdef _WIN32

// ---------------------------------------------------------------------------
// COM object (IUnknown + ISampleGrabberCB)
// ---------------------------------------------------------------------------

HRESULT DirectShowLayer::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv)
        return E_POINTER;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == IID_ISampleGrabberCB) {
        *ppv = static_cast<ISampleGrabberCB*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

ULONG DirectShowLayer::AddRef() {
    return m_comRefCount.fetch_add(1) + 1;
}

ULONG DirectShowLayer::Release() {
    // The immortal base reference keeps the count at >= 1, so this never
    // destroys the object.
    const ULONG previous = m_comRefCount.fetch_sub(1);
    return previous - 1;
}

HRESULT DirectShowLayer::SampleCallback(double /*sampleTime*/, IMediaSample* pSample) {
    if (!pSample || !m_handoff)
        return E_POINTER;

    BYTE* pData = nullptr;
    const DWORD length = static_cast<DWORD>(pSample->GetActualDataLength());
    HRESULT hr = pSample->GetPointer(&pData);
    if (FAILED(hr))
        return hr;
    if (!pData || length == 0)
        return E_FAIL;

    // Determine pixel format and size. Prefer the sample's own media type,
    // fall back to the graph output type cached after Run().
    PixelFormat pixelType = PixelFormat::Unknown;
    int width = 0;
    int height = 0;

    AM_MEDIA_TYPE* pSampleMt = nullptr;
    if (SUCCEEDED(pSample->GetMediaType(&pSampleMt)) && pSampleMt) {
        pixelType = samplePixelType(pSampleMt);
        videoInfoSize(pSampleMt, width, height);
        deleteMediaType(pSampleMt);
    }
    if (width <= 0 || height <= 0) {
        width = m_cachedWidth;
        height = m_cachedHeight;
        if (pixelType == PixelFormat::Unknown)
            pixelType = m_cachedPixelType;
    }

    // Expected buffer size for the negotiated layout.
    const size_t minBytes = [&pixelType, width, height]() -> size_t {
        switch (pixelType) {
            case PixelFormat::RGB24:  return static_cast<size_t>(width) * height * 3;
            case PixelFormat::RGB32:  return static_cast<size_t>(width) * height * 4;
            case PixelFormat::YUY2:
            case PixelFormat::UYVY:   return static_cast<size_t>(width) * height * 2;
            case PixelFormat::NV12:
            case PixelFormat::I420:
            case PixelFormat::YV12:   return static_cast<size_t>(width) * height * 3 / 2;
            case PixelFormat::Unknown:
        }
        return 0;
    }();

    if (minBytes == 0 || width <= 0 || height <= 0) {
        sgct::Log::Error("DirectShowLayer: sample without a supported pixel format, skipping\n");
        return E_FAIL;
    }

    if (length < minBytes) {
        sgct::Log::Error(std::format("DirectShowLayer: sample too small ({} < {})\n", length, minBytes));
        return E_FAIL;
    }

    // Source row stride for packed formats. Prefer the actual buffer layout when it is
    // consistent with the size, otherwise assume tightly packed rows. Planar YUV
    // formats below use their own plane offsets and are assumed tightly packed.
    const int bytesPerPixel = pixelType == PixelFormat::RGB24 ? 3
                        : (pixelType == PixelFormat::RGB32 ? 4
                        : ((pixelType == PixelFormat::YUY2 || pixelType == PixelFormat::UYVY) ? 2 : 0));
    size_t srcStride = static_cast<size_t>(width) * bytesPerPixel;
    if (bytesPerPixel > 0 && height > 0 && length % height == 0) {
        const size_t candidate = length / height;
        if (candidate >= srcStride)
            srcStride = candidate;
    }

    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    unsigned char* dst = pixels.data();
    switch (pixelType) {
        case PixelFormat::RGB24: {
            for (int y = 0; y < height; ++y) {
                const BYTE* srow = pData + static_cast<size_t>(y) * srcStride;
                unsigned char* drow = dst + static_cast<size_t>(y) * width * 4;
                for (int x = 0; x < width; ++x) {
                    drow[x * 4 + 0] = srow[x * 3 + 0]; // R
                    drow[x * 4 + 1] = srow[x * 3 + 1]; // G
                    drow[x * 4 + 2] = srow[x * 3 + 2]; // B
                    drow[x * 4 + 3] = 255;             // A
                }
            }
            break;
        }

        case PixelFormat::RGB32: { // stored as B,G,R,A (GDI order)
            for (int y = 0; y < height; ++y) {
                const BYTE* srow = pData + static_cast<size_t>(y) * srcStride;
                unsigned char* drow = dst + static_cast<size_t>(y) * width * 4;
                for (int x = 0; x < width; ++x) {
                    drow[x * 4 + 0] = srow[x * 4 + 2]; // R
                    drow[x * 4 + 1] = srow[x * 4 + 1]; // G
                    drow[x * 4 + 2] = srow[x * 4 + 0]; // B
                    drow[x * 4 + 3] = srow[x * 4 + 3]; // A
                }
            }
            break;
        }

        case PixelFormat::YUY2: { // each 4-byte group [Y Cb Y Cr] covers two pixels at columns x, x+1
            for (int y = 0; y < height; ++y) {
                const BYTE* srow = pData + static_cast<size_t>(y) * srcStride;
                unsigned char* drow = dst + static_cast<size_t>(y) * width * 4;
                for (int x = 0; x + 1 < width; x += 2) {
                    const int y0 = srow[x * 2 + 0];
                    const int cb = srow[x * 2 + 1] - 128;
                    const int y1 = srow[x * 2 + 2];
                    const int cr = srow[x * 2 + 3] - 128;
                    ycbcrToRgba(y0, cb, cr, drow + x * 4);
                    ycbcrToRgba(y1, cb, cr, drow + (x + 1) * 4);
                }
            }
            break;
        }

        case PixelFormat::UYVY: { // each 4-byte group [Cb Y Cr Y] covers two pixels at columns x, x+1
            for (int y = 0; y < height; ++y) {
                const BYTE* srow = pData + static_cast<size_t>(y) * srcStride;
                unsigned char* drow = dst + static_cast<size_t>(y) * width * 4;
                for (int x = 0; x + 1 < width; x += 2) {
                    const int cb = srow[x * 2 + 0] - 128;
                    const int y0 = srow[x * 2 + 1];
                    const int cr = srow[x * 2 + 2] - 128;
                    const int y1 = srow[x * 2 + 3];
                    ycbcrToRgba(y0, cb, cr, drow + x * 4);
                    ycbcrToRgba(y1, cb, cr, drow + (x + 1) * 4);
                }
            }
            break;
        }

        case PixelFormat::NV12: { // Y plane (w*h), then interleaved CbCr rows (h/2 of w bytes)
            const size_t yPlane = static_cast<size_t>(width) * height;
            const BYTE* pY = pData;
            const BYTE* pUV = pData + yPlane;
            for (int y = 0; y < height; ++y) {
                const int uvRow = y >> 1;
                unsigned char* drow = dst + static_cast<size_t>(y) * width * 4;
                for (int x = 0; x < width; ++x) {
                    const int yy = pY[static_cast<size_t>(y) * width + x];
                    const int cb = pUV[(static_cast<size_t>(uvRow) * width + x) * 2] - 128;
                    const int cr = pUV[(static_cast<size_t>(uvRow) * width + x) * 2 + 1] - 128;
                    ycbcrToRgba(yy, cb, cr, drow + x * 4);
                }
            }
            break;
        }

        case PixelFormat::I420: { // Y plane (w*h), then U and V planes ((w/2)*(h/2) each)
            const size_t yPlane = static_cast<size_t>(width) * height;
            const size_t uvPlane = static_cast<size_t>(width / 2) * (height / 2);
            const BYTE* pY = pData;
            const BYTE* pU = pData + yPlane;
            const BYTE* pV = pU + uvPlane;
            for (int y = 0; y < height; ++y) {
                const int uy = y >> 1;
                unsigned char* drow = dst + static_cast<size_t>(y) * width * 4;
                for (int x = 0; x < width; ++x) {
                    const int xx = x >> 1;
                    const int yy = pY[static_cast<size_t>(y) * width + x];
                    const int cb = pU[static_cast<size_t>(uy) * (width / 2) + xx] - 128;
                    const int cr = pV[static_cast<size_t>(uy) * (width / 2) + xx] - 128;
                    ycbcrToRgba(yy, cb, cr, drow + x * 4);
                }
            }
            break;
        }

        case PixelFormat::YV12: { // Y plane (w*h), then V and U planes ((w/2)*(h/2) each)
            const size_t yPlane = static_cast<size_t>(width) * height;
            const size_t uvPlane = static_cast<size_t>(width / 2) * (height / 2);
            const BYTE* pY = pData;
            const BYTE* pV = pData + yPlane;
            const BYTE* pU = pV + uvPlane;
            for (int y = 0; y < height; ++y) {
                const int uy = y >> 1;
                unsigned char* drow = dst + static_cast<size_t>(y) * width * 4;
                for (int x = 0; x < width; ++x) {
                    const int xx = x >> 1;
                    const int yy = pY[static_cast<size_t>(y) * width + x];
                    const int cb = pU[static_cast<size_t>(uy) * (width / 2) + xx] - 128;
                    const int cr = pV[static_cast<size_t>(uy) * (width / 2) + xx] - 128;
                    ycbcrToRgba(yy, cb, cr, drow + x * 4);
                }
            }
            break;
        }

        default: // unreachable - Unknown is filtered above
            return E_FAIL;
    }

    {
        std::lock_guard<std::mutex> lock(m_handoff->mutex);
        m_handoff->pixels = std::move(pixels);
        m_handoff->width = width;
        m_handoff->height = height;
        ++m_handoff->frameCount;
    }
    return S_OK;
}

HRESULT DirectShowLayer::BufferCallback(double /*sampleTime*/, BYTE* /*pBuffer*/, DWORD /*bufferLength*/) {
    // We registered for sample callbacks (mode 0), so this is never used.
    return E_NOTIMPL;
}

// ---------------------------------------------------------------------------
// Filter graph lifecycle
// ---------------------------------------------------------------------------

bool DirectShowLayer::buildAndRunGraph(const std::string& pathUtf8) {
    // COM must be initialized on this thread before any DirectShow call.
    CoInitializeEx(nullptr, COINIT_MULTITHREADED); // E_FAIL/RPC_E_CHANGED_MODE are fine

    const std::wstring path = toWideString(pathUtf8);
    if (path.empty())
        return false;

    HRESULT hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IGraphBuilder, reinterpret_cast<void**>(&m_graphBuilder));
    if (FAILED(hr) || !m_graphBuilder) {
        m_graphBuilder = nullptr;
        return false;
    }

    hr = m_graphBuilder->QueryInterface(IID_IMediaControl, reinterpret_cast<void**>(&m_mediaControl));
    if (FAILED(hr)) {
        releaseGraph();
        return false;
    }

    const bool isCapture = !m_captureVideoDevice.empty();
    if (isCapture) {
        // Live capture: create the filter for the device chosen in the UI.
        m_fileSourceFilter = createCaptureFilter(toWideString(m_captureVideoDevice));
        if (!m_fileSourceFilter) {
            sgct::Log::Error(std::format("DirectShowLayer: could not find video capture device '{}'\n",
                                         m_captureVideoDevice));
            releaseGraph();
            return false;
        }
    } else {
        // The file source filter may be registered under a different CLSID than the
        // classic one, so try both. Loading is attempted through IFileSourceFilter
        // first and then IMediaFile (not declared by this SDK).
        for (int attempt = 0; attempt < 2 && !m_fileSourceFilter; ++attempt) {
            const GUID& clsid = attempt == 0 ? kClsidFileSourceFilter : kClsidFileSourceAsync;
            IBaseFilter* pFilter = nullptr;
            if (FAILED(CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, IID_IBaseFilter,
                                        reinterpret_cast<void**>(&pFilter))) || !pFilter) {
                sgct::Log::Error(std::format("DirectShowLayer: could not create file source filter (attempt {})\n",
                                             attempt + 1));
                continue;
            }

            bool loaded = false;
            IFileSourceFilter* pFileSource = nullptr;
            if (SUCCEEDED(pFilter->QueryInterface(IID_IFileSourceFilter, reinterpret_cast<void**>(&pFileSource))) && pFileSource) {
                loaded = SUCCEEDED(pFileSource->Load(path.c_str(), nullptr));
                pFileSource->Release();
            }
            if (!loaded) {
                IMediaFile* pMediaFile = nullptr;
                if (SUCCEEDED(pFilter->QueryInterface(kIidIMediaFile, reinterpret_cast<void**>(&pMediaFile))) && pMediaFile) {
                    loaded = SUCCEEDED(pMediaFile->SetURL(path.c_str(), nullptr));
                    pMediaFile->Release();
                }
            }

            if (loaded) {
                m_fileSourceFilter = pFilter; // keep the reference
            } else {
                sgct::Log::Error(std::format("DirectShowLayer: file source filter could not load '{}'\n", pathUtf8));
                pFilter->Release();
            }
        }
    }
    if (!m_fileSourceFilter) {
        releaseGraph();
        return false;
    }

    hr = m_graphBuilder->AddFilter(m_fileSourceFilter, isCapture ? L"Capture Device" : L"File Source");
    if (FAILED(hr)) {
        releaseGraph();
        return false;
    }

    hr = CoCreateInstance(CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IBaseFilter, reinterpret_cast<void**>(&m_sampleGrabberFilter));
    if (FAILED(hr) || !m_sampleGrabberFilter) {
        m_sampleGrabberFilter = nullptr;
        releaseGraph();
        return false;
    }

    hr = m_graphBuilder->AddFilter(m_sampleGrabberFilter, L"Sample Grabber");
    if (FAILED(hr)) {
        releaseGraph();
        return false;
    }

    ISampleGrabber* pGrabber = nullptr;
    hr = m_sampleGrabberFilter->QueryInterface(IID_ISampleGrabber, reinterpret_cast<void**>(&pGrabber));
    if (SUCCEEDED(hr) && pGrabber) {
        // Do not constrain the grabber to a specific subtype. This machine's cameras
        // only offer YUY2/NV12/I420 and no converter filter is available in the graph,
        // so an RGB-only preference makes every connection fail with
        // VFW_E_TYPE_NOT_ACCEPTED; SampleCallback converts whatever arrives instead.

        // Mode 0: we receive IMediaSample pointers (with media type info).
        hr = pGrabber->SetCallback(this, 0);
        pGrabber->Release();
    }
    if (FAILED(hr)) {
        releaseGraph();
        return false;
    }

    // Connect the file source output to the grabber. ICaptureGraphBuilder2's
    // RenderStream inserts any needed decoders/converters; fall back to a
    // direct pin connection when it is unavailable or fails.
    IPin* pOutPin = nullptr;
    ICaptureGraphBuilder2* pCaptureBuilder = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ICaptureGraphBuilder2, reinterpret_cast<void**>(&pCaptureBuilder)))
        && pCaptureBuilder) {
        if (SUCCEEDED(pCaptureBuilder->SetFiltergraph(m_graphBuilder))) {
            hr = pCaptureBuilder->FindPin(m_fileSourceFilter, PINDIR_OUTPUT, &kPinCategoryVideo,
                                          &MEDIATYPE_Video, TRUE, 0, &pOutPin);
        }
    } else {
        sgct::Log::Error("DirectShowLayer: could not create capture graph builder\n");
    }
    if (!pOutPin)
        pOutPin = findPin(m_fileSourceFilter, PINDIR_OUTPUT); // last resort: any output pin

    bool connected = false;
    if (pCaptureBuilder && pOutPin) {
        hr = pCaptureBuilder->RenderStream(&kPinCategoryVideo, &MEDIATYPE_Video,
                                           pOutPin, nullptr, m_sampleGrabberFilter);
        connected = SUCCEEDED(hr);
    }
    if (!connected && pOutPin) {
        IPin* pInPin = findPin(m_sampleGrabberFilter, PINDIR_INPUT);
        if (pInPin) {
            hr = m_graphBuilder->Connect(pOutPin, pInPin);
            connected = SUCCEEDED(hr);
            pInPin->Release();
        }
    }
    if (pCaptureBuilder)
        pCaptureBuilder->Release();
    if (pOutPin)
        pOutPin->Release();

    if (!connected) {
        sgct::Log::Error("DirectShowLayer: could not connect file source to sample grabber\n");
        releaseGraph();
        return false;
    }

    hr = m_mediaControl->Run();
    if (FAILED(hr)) {
        releaseGraph();
        return false;
    }

    // Cache the negotiated output type - used when samples carry no media type.
    {
        AM_MEDIA_TYPE outType{};
        bool haveType = false;

        ISampleGrabber* pGrabber2 = nullptr;
        if (SUCCEEDED(m_sampleGrabberFilter->QueryInterface(IID_ISampleGrabber, reinterpret_cast<void**>(&pGrabber2)))
            && pGrabber2) {
            // Works on this machine's stack where IPin::ConnectionMediaType returns E_NOTIMPL.
            haveType = SUCCEEDED(pGrabber2->GetConnectedMediaType(&outType));
            pGrabber2->Release();
        }

        if (!haveType) {
            IPin* pGrabberInPin = findPin(m_sampleGrabberFilter, PINDIR_INPUT);
            if (pGrabberInPin) {
                haveType = SUCCEEDED(pGrabberInPin->ConnectionMediaType(&outType));
                pGrabberInPin->Release();
            }
        }

        if (haveType) {
            m_cachedPixelType = samplePixelType(&outType);
            videoInfoSize(&outType, m_cachedWidth, m_cachedHeight);
        } else {
            sgct::Log::Error("DirectShowLayer: could not read the grabber's connected media type\n");
        }
    }

    m_loadedFile = isCapture ? ("capture:" + m_captureVideoDevice) : pathUtf8;
    m_graphStart = std::chrono::steady_clock::now();
    sgct::Log::Info(std::format("DirectShowLayer: graph running for '{}'\n", m_loadedFile));
    if (isCapture && !m_captureAudioDevice.empty()) {
        // The audio device was chosen in the UI, but this layer renders video only.
        sgct::Log::Info(std::format("DirectShowLayer: audio capture device '{}' selected - audio routing pending\n",
                                    m_captureAudioDevice));
    }
    return true;
}

void DirectShowLayer::ensureGraph() {
    // Capture devices take precedence over file playback: when a video capture device
    // was chosen in the UI, the graph renders that live stream instead of the media
    // file at filepath(). The key identifies what the current graph is rendering.
    const std::string sourceKey = m_captureVideoDevice.empty() ? filepath() : ("capture:" + m_captureVideoDevice);
    if (sourceKey.empty())
        return;

    std::lock_guard<std::mutex> lock(m_graphMutex);

    // Already rendering this source - only check for a stall while not ready.
    if (m_graphBuilder && m_loadedFile == sourceKey) {
        if (!ready() && std::chrono::steady_clock::now() - m_graphStart > kStallTimeout) {
            sgct::Log::Error(std::format("DirectShowLayer: no frame received for '{}', giving up\n", sourceKey));
            releaseGraph();
            m_buildFailed = true;
        }
        return;
    }

    // A previous build for this exact source failed or stalled - don't spin on it.
    if (m_buildFailed && m_attemptedFile == sourceKey)
        return;

    // New source: rebuild the graph and drop the old frame so we don't show
    // stale content while loading.
    releaseGraph();
    if (renderData.texId > 0) {
        glDeleteTextures(1, &renderData.texId); // render thread - safe directly
        renderData.texId = 0;
        renderData.width = 0;
        renderData.height = 0;
    }

    m_attemptedFile = sourceKey;
    m_buildFailed = false;
    if (!buildAndRunGraph(sourceKey)) {
        m_buildFailed = true;
        sgct::Log::Error(std::format("DirectShowLayer: failed to build graph for '{}'\n", sourceKey));
    }
}

void DirectShowLayer::releaseGraph() {
    // Stop callbacks first so no sample can arrive while we tear down.
    if (m_sampleGrabberFilter) {
        ISampleGrabber* pGrabber = nullptr;
        if (SUCCEEDED(m_sampleGrabberFilter->QueryInterface(IID_ISampleGrabber, reinterpret_cast<void**>(&pGrabber)))
            && pGrabber) {
            pGrabber->SetCallback(nullptr, 0);
            pGrabber->Release();
        }
    }

    if (m_mediaControl)
        m_mediaControl->Stop();

    if (m_graphBuilder) {
        if (m_fileSourceFilter)
            m_graphBuilder->RemoveFilter(m_fileSourceFilter);
        if (m_sampleGrabberFilter)
            m_graphBuilder->RemoveFilter(m_sampleGrabberFilter);
    }

    if (m_fileSourceFilter) {
        m_fileSourceFilter->Release();
        m_fileSourceFilter = nullptr;
    }
    if (m_sampleGrabberFilter) {
        m_sampleGrabberFilter->Release();
        m_sampleGrabberFilter = nullptr;
    }
    if (m_mediaControl) {
        m_mediaControl->Release();
        m_mediaControl = nullptr;
    }
    if (m_graphBuilder) {
        m_graphBuilder->Release();
        m_graphBuilder = nullptr;
    }

    m_loadedFile.clear();
}

#endif // _WIN32
