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
#include <algorithm>
#include <cctype>
#include <cstring>
#include "audiosettings.h"
#include <utils/directshowpathresolver.h>

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
// Audio input device category CLSID (uuids.h: CLSID_AudioInputDeviceCategory) - same note as above.
const GUID kCatAudioInput{0x33d9a762, 0x90c8, 0x11d0, {0xbd, 0x43, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86}};
// Audio pin category (ksmedia.h: KSPIN_CATEGORY_AUDIO) - not declared by this machine's SDK headers.
const GUID kPinCategoryAudio{0x086325c7, 0x5fe7, 0x4f5e, {0x84, 0x6f, 0xc6, 0xff, 0x5e, 0x8b, 0x99, 0x8a}};
// PCM / IEEE float audio subformats (ksmedia.h: KSDATAFORMAT_SUBTYPE_PCM / _IEEE_FLOAT), used to
// resolve WAVEFORMATEXTENSIBLE formats. Not declared by this machine's SDK headers.
const GUID kSubtypePcm{0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID kSubtypeIeeeFloat{0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

// ---------------------------------------------------------------------------
// "No signal" detection for DeltaCast / Datapath capture cards.
//
// Both vendors expose a KS property set on their WDM capture filter that reports
// whether an input signal is present (IKsPropertySet, see ksproxy.h). The GUIDs
// and property IDs below are named after the vendor SDK constants:
//   DeltaCast : KSPROPSETID_DlCapture / DL_PROPERTY_SIGNAL_PRESENT         (bool/int)
//   Datapath  : GUID_DatapathVisionProperties / DATAPATH_PROP_SIGNAL_STATUS (status mask/bool)
// The values here are PLACEHOLDERS - replace them with the constants from the vendor SDK
// headers. Until then QuerySupported() reports "unsupported" and detection stays disabled,
// which is safe for every other capture device.
const GUID kKsPropSetDlCapture{0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // TODO: KSPROPSETID_DlCapture from the DeltaCast SDK
const DWORD kDlPropSignalPresent = 0; // TODO: DL_PROPERTY_SIGNAL_PRESENT from the DeltaCast SDK
const GUID kKsPropSetDatapathVision{0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}; // TODO: GUID_DatapathVisionProperties from the Datapath SDK
const DWORD kDatapathPropSignalStatus = 0; // TODO: DATAPATH_PROP_SIGNAL_STATUS from the Datapath SDK
// ksproxy.h: KSPROPERTY_SUPPORT_GET - bit set by IKsPropertySet::QuerySupported() when Get() is available.
constexpr DWORD kKsPropertySupportGet = 1;
// How often pollSignalPresence() reads the signal property (render thread, throttled).
constexpr std::chrono::milliseconds kSignalPollInterval{1000};

#pragma warning(push)
#pragma warning(disable : 5204) // COM interfaces have no destructor by design
// IMediaFile is not declared by this SDK - classic layout.
struct IMediaFile : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetFileType(LPOLESTR* pFileType) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetURL(LPCWSTR url, AM_MEDIA_TYPE* pmt) = 0;
};
#pragma warning(pop)

// Releases the format block (and any IUnknown) of an AM_MEDIA_TYPE whose structure itself is
// owned by the caller - e.g. a stack struct filled in by ISampleGrabber::GetConnectedMediaType
// or IPin::ConnectionMediaType, both of which deep-copy pbFormat with CoTaskMemAlloc. The
// format block must be released with CoTaskMemFree (or FreeMediaType), never free(): mixing
// allocators corrupts the CRT heap and crashes once audio samples start carrying media types.
void releaseMediaTypeFields(AM_MEDIA_TYPE& mt) {
    if (mt.cbFormat != 0 && mt.pbFormat) {
        CoTaskMemFree(mt.pbFormat);
        mt.cbFormat = 0;
        mt.pbFormat = nullptr;
    }
    if (mt.pUnk) {
        mt.pUnk->Release();
        mt.pUnk = nullptr;
    }
}

// Frees an AM_MEDIA_TYPE allocated by the DirectShow runtime with CoTaskMemAlloc, e.g. one
// returned by IMediaSample::GetMediaType or IEnumMediaTypes::Next (see DeleteMediaType docs).
void deleteMediaType(AM_MEDIA_TYPE* mt) {
    if (!mt)
        return;
    releaseMediaTypeFields(*mt);
    CoTaskMemFree(mt);
}

// How long a running graph may stay frameless before we give up on the file.
constexpr std::chrono::seconds kStallTimeout{10};
// Bounded audio ring capacity (jitter absorption between DirectShow and PortAudio) and the
// retry backoff for failed PortAudio stream opens.
constexpr int kAudioRingSeconds{2};
constexpr std::chrono::seconds kAudioRetryBackoff{2};
// A WASAPI input that opened fine but stops delivering samples entirely (driver quirk, unplugged
// endpoint) is replaced by the DirectShow WDM path after this long so audio keeps working.
constexpr std::chrono::seconds kAudioInputStallTimeout{3};

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

// Creates a capture filter for the device with the given friendly name from the given
// device category (video or audio input). Returns an added reference or nullptr when not found.
IBaseFilter* createCaptureFilter(const GUID& deviceCategory, const std::wstring& friendlyName) {
    IBaseFilter* result = nullptr;

    ICreateDevEnum* pSystemDevices = nullptr;
    if (FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                IID_ICreateDevEnum, reinterpret_cast<void**>(&pSystemDevices))) || !pSystemDevices) {
        return nullptr;
    }

    // This SDK's ICreateDevEnum enumerates monikers (not filters) - bind each one to
    // its property bag to read the device's friendly name.
    IEnumMoniker* pCategoryDevices = nullptr;
    const HRESULT hrEnum = pSystemDevices->CreateClassEnumerator(deviceCategory, &pCategoryDevices, 0);
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

// Resolves the effective PCM format tag of an audio WAVEFORMATEX, unwrapping
// WAVEFORMATEXTENSIBLE. Returns 0 when the format is not one we can convert.
unsigned short effectiveAudioFormatTag(const WAVEFORMATEX* wfx, DWORD formatSize) {
    if (!wfx || formatSize < sizeof(WAVEFORMATEX))
        return 0;
    unsigned short tag = wfx->wFormatTag;
    if (tag == WAVE_FORMAT_EXTENSIBLE && formatSize >= sizeof(WAVEFORMATEXTENSIBLE)) {
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wfx);
        if (IsEqualGUID(ext->SubFormat, kSubtypePcm))
            tag = WAVE_FORMAT_PCM;
        else if (IsEqualGUID(ext->SubFormat, kSubtypeIeeeFloat))
            tag = WAVE_FORMAT_IEEE_FLOAT;
        else
            return 0; // compressed subformat - not supported
    }
    return tag;
}

// Converts a grabbed audio sample to interleaved float32 in [-1, 1]. Supported: PCM8/PCM16,
// IEEE float32 and WAVEFORMATEXTENSIBLE wrapping one of those. Returns false for anything
// else (e.g. compressed codecs) - the caller then keeps playing video-only.
bool convertAudioToFloat32(const BYTE* data, DWORD length, const WAVEFORMATEX* wfx, DWORD formatSize, std::vector<float>& out) {
    if (!data || !wfx || wfx->nChannels == 0)
        return false;
    const unsigned short tag = effectiveAudioFormatTag(wfx, formatSize);
    const size_t bytesPerSample = wfx->wBitsPerSample / 8;
    if (bytesPerSample == 0)
        return false;

    const size_t nFrames = static_cast<size_t>(length) / (bytesPerSample * wfx->nChannels);
    out.resize(nFrames * wfx->nChannels);

    switch (tag) {
        case WAVE_FORMAT_PCM:
            if (wfx->wBitsPerSample == 8) { // unsigned PCM
                const auto* p = reinterpret_cast<const BYTE*>(data);
                for (size_t i = 0; i < out.size(); ++i)
                    out[i] = (static_cast<int>(p[i]) - 128) / 128.0f;
            } else if (wfx->wBitsPerSample == 16) {
                const auto* p = reinterpret_cast<const int16_t*>(data);
                for (size_t i = 0; i < out.size(); ++i)
                    out[i] = static_cast<float>(p[i]) / 32768.0f;
            } else {
                return false; // PCM with other bit depths is not supported
            }
            break;
        case WAVE_FORMAT_IEEE_FLOAT:
            if (wfx->wBitsPerSample != 32)
                return false;
            std::memcpy(out.data(), data, out.size() * sizeof(float));
            break;
        default:
            return false; // compressed/unknown codec - video-only fallback
    }
    return true;
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
    // Stop the audio output before tearing down the graph so no sample can be pushed
    // while the stream is closing (mirrors NdiLayer/OmtLayer).
    m_receiveAudio.store(false);
    closeAudioStream();
    closeAudioInput(); // and the low-latency WASAPI input, if one was open
    {
        std::lock_guard<std::mutex> lock(m_graphMutex);
        releaseGraph();
    }
    if (m_portAudioInitialized.load()) {
        Pa_Terminate();
        m_portAudioInitialized.store(false);
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

std::string DirectShowLayer::presetKey() const {
    return m_presetKey;
}

void DirectShowLayer::setPresetKey(const std::string& key) {
    if (m_presetKey != key) {
        m_presetKey = key;
        if (isMaster())
            setNeedSync();
    }
}

void DirectShowLayer::encodeTypeCore(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_presetKey);
}

void DirectShowLayer::decodeTypeCore(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_presetKey);
}

bool DirectShowLayer::ready() const {
    return renderData.texId > 0;
}

bool DirectShowLayer::hasTexture() const {
    return renderData.texId > 0;
}

void DirectShowLayer::update(bool updateRendering) {
#ifdef _WIN32
    // PortAudio lifecycle (mirrors NdiLayer/OmtLayer): initialize once when audio is enabled.
    if (m_isAudioEnabled && !m_portAudioInitialized.load())
        ensurePortAudioInitialized();

    const bool wantReceive = m_isAudioEnabled && m_portAudioInitialized.load();
    if (wantReceive != m_receiveAudio.load()) {
        m_receiveAudio.store(wantReceive);
        if (!wantReceive) {
            closeAudioStream(); // stop output immediately when audio is disabled
            closeAudioInput();  // and the low-latency WASAPI input stream too
            m_audioInputStalled = false; // a fresh attempt is allowed when audio is re-enabled
            m_audioRing.clear();
        }
    }

    if (wantReceive && !m_audioPathBuilt && !m_audioInputOpen.load()) {
        // Audio enabled at runtime for an explicit microphone: open the low-latency WASAPI input
        // directly instead of rebuilding the video graph just to add a DirectShow audio path. On
        // failure ensureGraph() below falls back to the WDM filter (full rebuild).
        std::string videoDevice, audioDevice;
        resolveCaptureDevices(videoDevice, audioDevice);
        if (!audioDevice.empty())
            openAudioInput(audioDevice);
    }

    if (wantReceive && m_audioInputOpen.load() && !m_audioStreamOpen.load()) {
        // Retry the PortAudio output stream while WASAPI input is feeding the ring (e.g., after a
        // transient failure at graph build). Backoff lives in openAudioStreamLocked().
        std::lock_guard<std::mutex> lock(m_audioStreamMutex);
        if (!m_audioStreamOpen.load())
            openAudioStreamLocked(m_audioInputRate.load(), m_audioInputChannels.load());
    }

    if (wantReceive && m_audioInputOpen.load()) {
        // Watchdog: a WASAPI input that opened fine but stops delivering samples entirely (driver
        // quirk, unplugged endpoint) is replaced by the DirectShow WDM path so audio keeps working.
        const long long lastMs = m_audioInputLastSampleMs.load(std::memory_order_relaxed);
        if (lastMs != 0) {
            const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch()).count();
            if (nowMs - lastMs > kAudioInputStallTimeout.count() * 1000LL) {
                sgct::Log::Warning("DirectShowLayer: WASAPI input stopped delivering samples - falling back to the DirectShow audio path\n");
                m_audioInputStalled = true; // skip WASAPI for the next graph build
                closeAudioInput();
            }
        }
    }

    ensureGraph(); // rebuilds the graph once if an audio path must be added or removed
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
// Per-grabber sample callbacks (IUnknown + ISampleGrabberCB)
// ---------------------------------------------------------------------------

GrabberCallback::GrabberCallback(DirectShowLayer* layer, Role role) : m_layer(layer), m_role(role) {
}

HRESULT GrabberCallback::QueryInterface(REFIID riid, void** ppv) {
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

ULONG GrabberCallback::AddRef() {
    return m_refCount.fetch_add(1) + 1;
}

ULONG GrabberCallback::Release() {
    // The immortal base reference keeps the count at >= 1, so this never
    // destroys the object - destruction is owned by DirectShowLayer.
    const ULONG previous = m_refCount.fetch_sub(1);
    return previous - 1;
}

HRESULT GrabberCallback::SampleCallback(double /*sampleTime*/, IMediaSample* pMediaSample) {
    if (!m_layer || !pMediaSample)
        return E_POINTER;

    // Routing is structural: the video grabber's callback only ever sees video
    // samples and the audio grabber's only audio samples, so no media-type sniffing.
    if (m_role == Role::Audio) {
        BYTE* pData = nullptr;
        const DWORD length = static_cast<DWORD>(pMediaSample->GetActualDataLength());
        HRESULT hr = pMediaSample->GetPointer(&pData);
        if (FAILED(hr))
            return hr;
        if (!pData || length == 0)
            return E_FAIL;

        // Prefer the sample's own media type when it carries one - WDM/KS capture
        // samples often do not, and handleAudioSample() then falls back to the
        // format cached from the grabber's connected pin.
        AM_MEDIA_TYPE* pMt = nullptr;
        if (SUCCEEDED(pMediaSample->GetMediaType(&pMt)) && pMt) {
            m_layer->handleAudioSample(pData, length, pMt);
            deleteMediaType(pMt);
        } else {
            m_layer->handleAudioSample(pData, length, nullptr);
        }
        return S_OK;
    }

    return m_layer->handleVideoSample(pMediaSample);
}

HRESULT DirectShowLayer::handleVideoSample(IMediaSample* pSample) {
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
                    // NV12 is YCbCr 4:2:0 - the UV plane holds only w/2 CbCr pairs per row,
                    // one pair covering two horizontal pixels. Indexing it with a full pixel
                    // column (as in 4:2:2) reads up to half a frame past the end of the sample
                    // buffer and crashes on NV12 sources such as OBS Virtual Camera.
                    const int xx = x >> 1;
                    const int yy = pY[static_cast<size_t>(y) * width + x];
                    const int cb = pUV[(static_cast<size_t>(uvRow) * (width / 2) + xx) * 2] - 128;
                    const int cr = pUV[(static_cast<size_t>(uvRow) * (width / 2) + xx) * 2 + 1] - 128;
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

// ---------------------------------------------------------------------------
// Filter graph lifecycle
// ---------------------------------------------------------------------------

bool DirectShowLayer::buildVideoPath(const std::string& videoDevice) {
    // Adds the video source to the graph and connects it to the sample grabber.
    // m_fileSourceFilter must already be created (capture or file source).
    HRESULT hr = S_OK;

    hr = m_graphBuilder->AddFilter(m_fileSourceFilter, !videoDevice.empty() ? L"Capture Device" : L"File Source");
    if (FAILED(hr)) {
        return false;
    }

    hr = CoCreateInstance(CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IBaseFilter, reinterpret_cast<void**>(&m_sampleGrabberFilter));
    if (FAILED(hr) || !m_sampleGrabberFilter) {
        m_sampleGrabberFilter = nullptr;
        return false;
    }

    hr = m_graphBuilder->AddFilter(m_sampleGrabberFilter, L"Sample Grabber");
    if (FAILED(hr)) {
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
        hr = pGrabber->SetCallback(&m_videoCallback, 0);
        pGrabber->Release();
    }
    if (FAILED(hr)) {
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
        return false;
    }
    return true;
}

bool DirectShowLayer::buildAndRunGraph(const std::string& pathUtf8, const std::string& videoDevice, const std::string& audioDevice) {
    // COM must be initialized on this thread before any DirectShow call.
    CoInitializeEx(nullptr, COINIT_MULTITHREADED); // E_FAIL/RPC_E_CHANGED_MODE are fine

    // Fresh build - reset per-graph audio state.
    m_audioPathBuilt = false;
    m_audioPathUnavailable = false;
    m_unsupportedAudioLogged.store(false);
    m_cachedAudioRate = 0;
    m_cachedAudioChannels = 0;
    m_cachedAudioFormatSize = 0;

    const bool isCapture = !videoDevice.empty();
    // An audio-only preset has no video device but does have a microphone: the graph then
    // carries just the audio capture path and never produces video frames.
    const bool audioOnly = !isCapture && !audioDevice.empty();

    std::wstring path;
    if (!audioOnly) {
        path = toWideString(pathUtf8);
        if (path.empty())
            return false;
    }

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

    if (isCapture) {
        // Live capture: create the filter for this machine's resolved video device.
        m_fileSourceFilter = createCaptureFilter(kCatVideoInput, toWideString(videoDevice));
        if (!m_fileSourceFilter) {
            sgct::Log::Error(std::format("DirectShowLayer: could not find video capture device '{}'\n",
                                         videoDevice));
            releaseGraph();
            return false;
        }
    } else if (!audioOnly) {
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
    if (!audioOnly && !m_fileSourceFilter) {
        releaseGraph();
        return false;
    }

    // Video path (source -> sample grabber). Skipped entirely for audio-only graphs.
    if (!audioOnly && !buildVideoPath(videoDevice)) {
        releaseGraph();
        return false;
    }

    // Audio path: a second sample grabber fed from the source's audio pin or an explicit
    // capture device. Non-fatal for video graphs (any failure leaves them video-only), but
    // fatal for an audio-only graph - there is nothing else to render then. An explicit
    // microphone is preferred through a low-latency WASAPI input stream: the WDM/DirectShow
    // audio filter delivers ~500ms blocks and adds that much steady-state latency (e.g., C925).
    if (m_isAudioEnabled) {
        bool audioBuilt = false;
        if (!audioDevice.empty()) {
            if (!m_audioInputStalled && openAudioInput(audioDevice)) {
                audioBuilt = true; // no DirectShow filter needed - m_audioPathBuilt stays false
            } else {
                sgct::Log::Info(std::format("DirectShowLayer: WASAPI input unavailable for '{}' - using the DirectShow audio path\n", audioDevice));
                audioBuilt = buildAudioPath(audioDevice);
            }
        } else {
            closeAudioInput(); // no explicit microphone for this source - stop any stale input stream
            audioBuilt = buildAudioPath("");
        }
        m_audioInputStalled = false; // consumed (or not applicable) by this build
        if (!audioBuilt && audioOnly) {
            sgct::Log::Error("DirectShowLayer: could not build the audio capture path\n");
            releaseGraph();
            return false;
        }
    } else {
        closeAudioInput(); // audio disabled - make sure no stale input stream keeps running
    }

    hr = m_mediaControl->Run();
    if (FAILED(hr)) {
        // An audio-only source captured through WASAPI leaves the DirectShow graph empty; a failed
        // Run() there must not mark the build failed while audio is already flowing.
        const bool wasapiAudioOnly = audioOnly && m_audioInputOpen.load();
        if (!wasapiAudioOnly) {
            releaseGraph();
            return false;
        }
    }

    // Cache the negotiated output type - used when samples carry no media type.
    if (!audioOnly) {
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

        // outType is a stack struct; only its format block was allocated by DirectShow.
        releaseMediaTypeFields(outType);
    }

    m_loadedFile = isCapture ? ("capture:" + videoDevice) : pathUtf8;
    if (isCapture) {
        // Live capture: look for a DeltaCast/Datapath signal property on the filter so we can log
        // "no signal" / "signal restored" transitions while the graph runs.
        setupSignalDetection();
    }
    m_graphStart = std::chrono::steady_clock::now();
    sgct::Log::Info(std::format("DirectShowLayer: graph running for '{}'\n", m_loadedFile));
    if (m_isAudioEnabled && !m_audioPathBuilt && !m_audioInputOpen.load()) {
        // Audio was requested but no usable audio pin/connection exists for this source.
        sgct::Log::Info("DirectShowLayer: no audio path available - playing video-only\n");
    }
    return true;
}

// "No signal" detection for DeltaCast / Datapath capture cards (see kKsPropSetDlCapture /
// kKsPropSetDatapathVision). The card's WDM filter exposes IKsPropertySet; when it supports one
// of the vendor property sets we keep a reference to it and remember which signal-status
// property to poll. Render thread only, during graph build for capture devices.
void DirectShowLayer::setupSignalDetection() {
    if (m_signalPropSet) {
        m_signalPropSet->Release();
        m_signalPropSet = nullptr;
    }
    m_signalVendor = SignalVendor::None;
    m_signalState.store(SignalState::Unknown);
    m_lastSignalPoll = {}; // allow an immediate first read after (re)build

    struct Spec {
        const char* name;
        SignalVendor vendor;
        GUID setGuid;
        DWORD propId;
    };
    static const Spec kSpecs[] = {
        {"DeltaCast", SignalVendor::DeltaCast, kKsPropSetDlCapture, kDlPropSignalPresent},
        {"Datapath",  SignalVendor::Datapath,  kKsPropSetDatapathVision, kDatapathPropSignalStatus},
    };

    if (!m_fileSourceFilter) {
        return; // file playback - nothing to poll
    }

    IKsPropertySet* pProps = nullptr;
    if (FAILED(m_fileSourceFilter->QueryInterface(IID_IKsPropertySet, reinterpret_cast<void**>(&pProps))) || !pProps) {
        sgct::Log::Debug("DirectShowLayer: capture filter does not expose IKsPropertySet - signal detection disabled\n");
        return;
    }

    for (const Spec& spec : kSpecs) {
        DWORD support = 0;
        if (SUCCEEDED(pProps->QuerySupported(spec.setGuid, spec.propId, &support)) && (support & kKsPropertySupportGet)) {
            m_signalVendor = spec.vendor;
            m_signalSetGuid = spec.setGuid;
            m_signalPropId = spec.propId;
            m_signalPropSet = pProps; // keep the reference until releaseGraph()
            sgct::Log::Debug(std::format("DirectShowLayer: {} signal property supported - polling for signal presence\n", spec.name));
            return;
        }
    }

    sgct::Log::Debug("DirectShowLayer: no DeltaCast/Datapath signal property on this capture device - detection disabled\n");
    pProps->Release();
}

// Reads the vendor's signal-status property (throttled to kSignalPollInterval) and logs state
// transitions. Both vendors report presence as a boolean/integer or status mask, so non-zero
// means "signal present". Render thread only; called from ensureGraph() while holding m_graphMutex.
void DirectShowLayer::pollSignalPresence() {
    if (!m_signalPropSet || m_signalVendor == SignalVendor::None) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (now - m_lastSignalPoll < kSignalPollInterval) {
        return;
    }
    m_lastSignalPoll = now;

    DWORD value = 0;
    DWORD returned = 0;
    if (FAILED(m_signalPropSet->Get(m_signalSetGuid, m_signalPropId, nullptr, 0, &value, sizeof(value), &returned))) {
        return; // transient failure - keep the last known state
    }

    const char* vendorName = m_signalVendor == SignalVendor::DeltaCast ? "DeltaCast" : "Datapath";
    const SignalState next = value != 0 ? SignalState::Present : SignalState::Absent;
    const SignalState prev = m_signalState.exchange(next);
    if (prev == next) {
        return;
    }

    switch (next) {
    case SignalState::Present:
        if (prev == SignalState::Unknown) {
            sgct::Log::Debug(std::format("DirectShowLayer: signal present on '{}' ({})\n", m_loadedFile, vendorName));
        } else {
            sgct::Log::Debug(std::format("DirectShowLayer: signal restored on '{}' ({}) - no-signal ended\n", m_loadedFile, vendorName));
        }
        break;
    case SignalState::Absent:
        sgct::Log::Debug(std::format("DirectShowLayer: NO SIGNAL detected on '{}' ({})\n", m_loadedFile, vendorName));
        break;
    default:
        break; // Unknown is only ever the previous state, never the new one
    }
}

void DirectShowLayer::ensureGraph() {
    // Capture devices take precedence over file playback: when a video capture device
    // was chosen in the UI, the graph renders that live stream instead of the media
    // file at filepath(). The key identifies what the current graph is rendering.
    std::string videoDevice, audioDevice;
    const bool fromPreset = resolveCaptureDevices(videoDevice, audioDevice);

    if (fromPreset && videoDevice.empty() && audioDevice.empty()) {
        // This machine intentionally has no capture for this setup - stay idle and never
        // fall back to file playback. Tear down any graph from a previous resolution.
        std::lock_guard<std::mutex> lock(m_graphMutex);
        closeAudioInput(); // stop the low-latency WASAPI input too, if one was open
        if (m_graphBuilder) {
            releaseGraph();
            if (renderData.texId > 0) {
                glDeleteTextures(1, &renderData.texId); // render thread - safe directly
                renderData.texId = 0;
                renderData.width = 0;
                renderData.height = 0;
            }
        }
        return;
    }

    const bool isCapture = !videoDevice.empty();
    // An audio-only preset (microphone without a video device) never produces frames, so
    // ready() stays false and the stall timeout below must not apply to it.
    const bool audioOnly = !isCapture && !audioDevice.empty();
    const std::string sourceKey = isCapture ? ("capture:" + videoDevice)
                          : (audioOnly ? ("audio-capture:" + audioDevice) : filepath());
    if (sourceKey.empty())
        return;

    std::lock_guard<std::mutex> lock(m_graphMutex);

    // Already rendering this source - only check for a stall while not ready. Rebuild once
    // when the audio path presence no longer matches the enabled state (audio toggled at
    // runtime). m_audioPathUnavailable stops us from retrying sources without an audio pin.
    if (m_graphBuilder && m_loadedFile == sourceKey) {
        // An open WASAPI input stream satisfies the audio requirement without any DirectShow filter,
        // so it must not trigger a rebuild when audio is toggled at runtime.
        const bool audioSatisfied = m_audioPathBuilt || (m_audioInputOpen.load() && !audioDevice.empty());
        const bool audioMismatch = (m_isAudioEnabled && !audioSatisfied && !m_audioPathUnavailable)
                                || (!m_isAudioEnabled && (m_audioPathBuilt || m_audioInputOpen.load()));
        if (!audioMismatch) {
            if (!ready() && !audioOnly && std::chrono::steady_clock::now() - m_graphStart > kStallTimeout) {
                sgct::Log::Error(std::format("DirectShowLayer: no frame received for '{}', giving up\n", sourceKey));
                releaseGraph();
                m_buildFailed = true;
            }
            pollSignalPresence(); // throttled internally; no-op unless a DeltaCast/Datapath signal property was found
            return;
        }
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
    if (!buildAndRunGraph(sourceKey, videoDevice, audioDevice)) {
        m_buildFailed = true;
        sgct::Log::Error(std::format("DirectShowLayer: failed to build graph for '{}'\n", sourceKey));
    } else {
        pollSignalPresence(); // first read right after the graph started (throttled afterwards)
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
    if (m_audioGrabberFilter) {
        ISampleGrabber* pGrabber = nullptr;
        if (SUCCEEDED(m_audioGrabberFilter->QueryInterface(IID_ISampleGrabber, reinterpret_cast<void**>(&pGrabber)))
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
        if (m_audioGrabberFilter)
            m_graphBuilder->RemoveFilter(m_audioGrabberFilter);
        if (m_audioSourceFilter)
            m_graphBuilder->RemoveFilter(m_audioSourceFilter);
    }

    if (m_fileSourceFilter) {
        m_fileSourceFilter->Release();
        m_fileSourceFilter = nullptr;
    }
    if (m_sampleGrabberFilter) {
        m_sampleGrabberFilter->Release();
        m_sampleGrabberFilter = nullptr;
    }
    if (m_audioGrabberFilter) {
        m_audioGrabberFilter->Release();
        m_audioGrabberFilter = nullptr;
    }
    if (m_audioSourceFilter) {
        m_audioSourceFilter->Release();
        m_audioSourceFilter = nullptr;
    }
    if (m_mediaControl) {
        m_mediaControl->Release();
        m_mediaControl = nullptr;
    }
    if (m_graphBuilder) {
        m_graphBuilder->Release();
        m_graphBuilder = nullptr;
    }

    if (m_signalPropSet) {
        m_signalPropSet->Release();
        m_signalPropSet = nullptr;
    }
    m_signalVendor = SignalVendor::None;
    m_signalState.store(SignalState::Unknown);

    m_loadedFile.clear();
    m_audioPathBuilt = false;
    m_audioPathUnavailable = false;
}

// ---------------------------------------------------------------------------
// Audio path + PortAudio output (mirrors NdiLayer/OmtLayer)
// ---------------------------------------------------------------------------

bool DirectShowLayer::AudioRing::setFormat(int ch, int sampleRate) {
    std::lock_guard<std::mutex> lock(mutex);
    if (ch <= 0 || sampleRate <= 0)
        return false;
    const size_t newMaxFrames = static_cast<size_t>(sampleRate) * kAudioRingSeconds; // bounded jitter buffer
    if (!data.empty() && channels == ch && maxFrames == newMaxFrames)
        return true; // same layout - keep buffered audio
    data.assign(newMaxFrames * static_cast<size_t>(ch), 0.0f);
    channels = ch;
    head = tail = frames = 0;
    maxFrames = newMaxFrames;
    return true;
}

void DirectShowLayer::AudioRing::push(const float* interleaved, size_t nFrames) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!interleaved || nFrames == 0 || maxFrames == 0 || channels <= 0)
        return;
    const size_t ch = static_cast<size_t>(channels);

    // Never buffer more than the capacity: drop oldest (existing first, then incoming).
    if (nFrames >= maxFrames) {
        interleaved += (nFrames - maxFrames) * ch; // keep only the newest frames of this chunk
        nFrames = maxFrames;
        tail = head;                               // discard everything already buffered
        frames = 0;
    } else if (frames + nFrames > maxFrames) {
        const size_t drop = frames + nFrames - maxFrames;
        tail = (tail + drop) % maxFrames;
        frames -= drop;
    }

    for (size_t f = 0; f < nFrames; ++f) {
        std::copy_n(interleaved + f * ch, ch, data.data() + head * ch);
        head = (head + 1) % maxFrames;
    }
    frames += nFrames;
}

size_t DirectShowLayer::AudioRing::pop(float* out, int outChannels, size_t maxNFrames, float volume) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!out || outChannels <= 0 || frames == 0 || maxFrames == 0 || channels <= 0)
        return 0;
    const size_t n = (frames < maxNFrames) ? frames : maxNFrames;
    const size_t inCh = static_cast<size_t>(channels);
    for (size_t f = 0; f < n; ++f) {
        const float* src = data.data() + tail * inCh;
        float* dst = out + f * static_cast<size_t>(outChannels);
        for (int oc = 0; oc < outChannels; ++oc) {
            // Map output channel onto an input channel (simple wrap for down/upmix - mirrors OmtLayer).
            const int ic = (oc < channels) ? oc : (oc % channels);
            dst[oc] = src[ic] * volume;
        }
        tail = (tail + 1) % maxFrames;
    }
    frames -= n;
    return n;
}

void DirectShowLayer::AudioRing::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    head = tail = frames = 0;
}

bool DirectShowLayer::buildAudioPath(const std::string& audioDevice) {
    // Non-fatal by contract: any failure just leaves the graph video-only.
    m_audioPathBuilt = false;
    m_audioPathUnavailable = false;

    IPin* pOutPin = nullptr; // pin feeding the audio grabber (added reference)

    if (!audioDevice.empty()) {
        // Explicit microphone for this machine: add an audio capture filter.
        IBaseFilter* pAudioCapture = createCaptureFilter(kCatAudioInput, toWideString(audioDevice));
        if (!pAudioCapture) {
            sgct::Log::Error(std::format("DirectShowLayer: could not find audio capture device '{}'\n", audioDevice));
        } else if (FAILED(m_graphBuilder->AddFilter(pAudioCapture, L"Audio Capture"))) {
            pAudioCapture->Release();
        } else {
            m_audioSourceFilter = pAudioCapture; // keep the reference for releaseGraph()
            pOutPin = findPin(m_audioSourceFilter, PINDIR_OUTPUT);
        }
    }

    if (!pOutPin) {
        // The source's own audio pin (file playback or a capture filter with a built-in mic).
        if (!m_fileSourceFilter) {
            // Audio-only graph - there is no video/source filter to look for an audio pin on.
            m_audioPathUnavailable = true;
            return false;
        }
        ICaptureGraphBuilder2* pCaptureBuilder = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_ICaptureGraphBuilder2, reinterpret_cast<void**>(&pCaptureBuilder)))
            && pCaptureBuilder) {
            if (SUCCEEDED(pCaptureBuilder->SetFiltergraph(m_graphBuilder))) {
                pCaptureBuilder->FindPin(m_fileSourceFilter, PINDIR_OUTPUT, &kPinCategoryAudio,
                                         &MEDIATYPE_Audio, TRUE, 0, &pOutPin);
            }
            pCaptureBuilder->Release();
        }

        if (!pOutPin) {
            // Last resort: any output pin whose media type is audio.
            IEnumPins* pEnum = nullptr;
            if (SUCCEEDED(m_fileSourceFilter->EnumPins(&pEnum)) && pEnum) {
                while (true) {
                    IPin* pPin = nullptr;
                    ULONG fetched = 0;
                    if (FAILED(pEnum->Next(1, &pPin, &fetched)) || fetched != 1 || !pPin)
                        break;
                    PIN_DIRECTION pinDir = PINDIR_INPUT;
                    bool isAudioOut = false;
                    if (SUCCEEDED(pPin->QueryDirection(&pinDir)) && pinDir == PINDIR_OUTPUT) {
                        // Enumerate the pin's supported media types and look for audio.
                        IEnumMediaTypes* pMtEnum = nullptr;
                        if (SUCCEEDED(pPin->EnumMediaTypes(&pMtEnum)) && pMtEnum) {
                            while (true) {
                                AM_MEDIA_TYPE* pMt = nullptr;
                                ULONG fetchedMt = 0;
                                if (FAILED(pMtEnum->Next(1, &pMt, &fetchedMt)) || fetchedMt != 1 || !pMt)
                                    break;
                                const bool audioType = IsEqualGUID(pMt->majortype, MEDIATYPE_Audio);
                                deleteMediaType(pMt);
                                if (audioType) {
                                    isAudioOut = true;
                                    break;
                                }
                            }
                            pMtEnum->Release();
                        }
                    }
                    if (!isAudioOut)
                        pPin->Release();
                    else
                        pOutPin = pPin; // keep the reference from Next()
                    if (pOutPin)
                        break;
                }
                pEnum->Release();
            }
        }

        if (!pOutPin) {
            m_audioPathUnavailable = true;
            sgct::Log::Info("DirectShowLayer: source has no usable audio pin - playing video-only\n");
            return false;
        }
    }

    // Create the audio sample grabber (shares this callback object with the video one).
    IBaseFilter* pGrabberFilter = nullptr;
    if (FAILED(CoCreateInstance(CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IBaseFilter, reinterpret_cast<void**>(&pGrabberFilter))) || !pGrabberFilter) {
        pOutPin->Release();
        return false;
    }

    bool grabberAdded = false;
    bool ok = false;
    if (SUCCEEDED(m_graphBuilder->AddFilter(pGrabberFilter, L"Audio Sample Grabber"))) {
        grabberAdded = true;
        ISampleGrabber* pGrabber = nullptr;
        if (SUCCEEDED(pGrabberFilter->QueryInterface(IID_ISampleGrabber, reinterpret_cast<void**>(&pGrabber))) && pGrabber) {
            // Mode 0: IMediaSample pointers so the audio format can be read per sample.
            ok = SUCCEEDED(pGrabber->SetCallback(&m_audioCallback, 0));
            pGrabber->Release();
        }

        if (ok) {
            IPin* pInPin = findPin(pGrabberFilter, PINDIR_INPUT);
            ICaptureGraphBuilder2* pCaptureBuilder = nullptr;
            bool connected = false;
            if (pInPin && SUCCEEDED(CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER,
                                                     IID_ICaptureGraphBuilder2, reinterpret_cast<void**>(&pCaptureBuilder)))
                && pCaptureBuilder) {
                if (SUCCEEDED(pCaptureBuilder->SetFiltergraph(m_graphBuilder))) {
                    // RenderStream inserts any needed decoders/converters; fall back to a
                    // direct pin connection when it is unavailable or fails.
                    connected = SUCCEEDED(pCaptureBuilder->RenderStream(&kPinCategoryAudio, &MEDIATYPE_Audio,
                                                                        pOutPin, nullptr, pGrabberFilter));
                }
            }
            if (!connected && pInPin) {
                connected = SUCCEEDED(m_graphBuilder->Connect(pOutPin, pInPin));
            }
            if (pCaptureBuilder)
                pCaptureBuilder->Release();
            if (pInPin)
                pInPin->Release();

            ok = connected;
        }
    }

    pOutPin->Release();

    if (!ok) {
        // Roll back the partially built audio path - the graph stays video-only.
        sgct::Log::Error("DirectShowLayer: could not connect the audio sample grabber - playing video-only\n");
        if (grabberAdded)
            m_graphBuilder->RemoveFilter(pGrabberFilter);
        if (m_audioSourceFilter)
            m_graphBuilder->RemoveFilter(m_audioSourceFilter);
        pGrabberFilter->Release();
        if (m_audioSourceFilter) {
            m_audioSourceFilter->Release();
            m_audioSourceFilter = nullptr;
        }
        return false;
    }

    m_audioGrabberFilter = pGrabberFilter; // keep the reference for releaseGraph()
    m_audioPathBuilt = true;

    // Cache the negotiated audio format now (the pin is connected, this runs before Run()).
    // WDM/KS capture devices often deliver samples whose GetMediaType() fails, and
    // handleAudioSample() falls back to this cache - mirroring what buildAndRunGraph()
    // does for the video grabber. Without it, such sources would drop every sample.
    AM_MEDIA_TYPE outType{};
    bool haveType = false;
    ISampleGrabber* pFormatGrabber = nullptr;
    if (SUCCEEDED(pGrabberFilter->QueryInterface(IID_ISampleGrabber, reinterpret_cast<void**>(&pFormatGrabber)))
        && pFormatGrabber) {
        // Works on this machine's stack where IPin::ConnectionMediaType returns E_NOTIMPL.
        haveType = SUCCEEDED(pFormatGrabber->GetConnectedMediaType(&outType));
        pFormatGrabber->Release();
    }
    if (!haveType) {
        IPin* pInPin2 = findPin(pGrabberFilter, PINDIR_INPUT);
        if (pInPin2) {
            haveType = SUCCEEDED(pInPin2->ConnectionMediaType(&outType));
            pInPin2->Release();
        }
    }
    if (haveType && outType.pbFormat && outType.cbFormat >= sizeof(WAVEFORMATEX)) {
        const WAVEFORMATEX* wfx = reinterpret_cast<const WAVEFORMATEX*>(outType.pbFormat);
        m_cachedAudioRate = static_cast<int>(wfx->nSamplesPerSec);
        m_cachedAudioChannels = static_cast<int>(wfx->nChannels);
        const DWORD copySize = std::min<DWORD>(outType.cbFormat, sizeof(m_cachedAudioFormat));
        std::copy_n(outType.pbFormat, static_cast<int>(copySize), m_cachedAudioFormat.begin());
        m_cachedAudioFormatSize = copySize;
    } else {
        sgct::Log::Error("DirectShowLayer: could not read the audio grabber's connected media type\n");
    }
    // outType is a stack struct filled in by GetConnectedMediaType/ConnectionMediaType; only its
    // format block was allocated by DirectShow, so release just the fields (no-op if empty).
    releaseMediaTypeFields(outType);

    sgct::Log::Info(std::format("DirectShowLayer: audio path built ({} Hz, {} ch)\n",
                                m_cachedAudioRate, m_cachedAudioChannels));
    return true;
}

void DirectShowLayer::handleAudioSample(const BYTE* pData, DWORD length, const AM_MEDIA_TYPE* pMt) {
    // Audio is only consumed while enabled and PortAudio is ready (set by update()).
    if (!m_receiveAudio.load())
        return;

    // Resolve the sample's format - prefer the sample's own media type.
    int rate = 0;
    int channels = 0;
    const WAVEFORMATEX* wfx = nullptr;
    DWORD wfxSize = 0;
    if (pMt && pMt->pbFormat && pMt->cbFormat >= sizeof(WAVEFORMATEX)) {
        wfx = reinterpret_cast<const WAVEFORMATEX*>(pMt->pbFormat);
        wfxSize = pMt->cbFormat;
        rate = static_cast<int>(wfx->nSamplesPerSec);
        channels = static_cast<int>(wfx->nChannels);
    }
    if (rate <= 0 || channels <= 0) {
        // Sample carried no usable media type - fall back to the cached pin format.
        if (m_cachedAudioFormatSize < sizeof(WAVEFORMATEX))
            return;
        wfx = reinterpret_cast<const WAVEFORMATEX*>(m_cachedAudioFormat.data());
        wfxSize = m_cachedAudioFormatSize;
        rate = m_cachedAudioRate;
        channels = m_cachedAudioChannels;
    }

    // Cache the format for samples that carry no media type.
    if (wfx != reinterpret_cast<const WAVEFORMATEX*>(m_cachedAudioFormat.data()) && wfxSize <= m_cachedAudioFormat.size()) {
        std::copy_n(reinterpret_cast<const BYTE*>(wfx), static_cast<int>(wfxSize), m_cachedAudioFormat.begin());
        m_cachedAudioFormatSize = wfxSize;
        m_cachedAudioRate = rate;
        m_cachedAudioChannels = channels;
    }

    // Convert to interleaved float32 (PCM8/16, IEEE float32, extensible PCM). Unsupported
    // codecs are skipped - the layer keeps playing video-only.
    std::vector<float> converted;
    if (!convertAudioToFloat32(pData, length, wfx, wfxSize, converted)) {
        if (m_unsupportedAudioLogged.exchange(true) == false) {
            sgct::Log::Warning(std::format("DirectShowLayer: unsupported audio format (tag 0x{:04X}, {} bits) - continuing video-only\n",
                                           wfx->wFormatTag, wfx->wBitsPerSample));
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_audioStreamMutex);
        if (m_portAudioInitialized.load()) {
            const bool formatChanged = (m_paSampleRate != rate) || (m_audioSourceChannels != channels);
            if (!m_audioStreamOpen.load()) {
                openAudioStreamLocked(rate, channels); // lazy open once the first usable sample arrives
            } else if (formatChanged) {
                closeAudioStreamLocked();
                openAudioStreamLocked(rate, channels);
            }
        }
        if (m_audioStreamOpen.load() && m_audioStreamStarted.load()) {
            const size_t nFrames = converted.size() / static_cast<size_t>(channels);
            if (nFrames > 0) {
                m_audioRing.setFormat(channels, rate);
                m_audioRing.push(converted.data(), nFrames);
            }
        }
    }
}

void DirectShowLayer::ensurePortAudioInitialized() {
    if (m_portAudioInitialized.load())
        return;
    const PaError err = Pa_Initialize();
    if (err != paNoError) {
        sgct::Log::Error(std::format("DirectShowLayer: PortAudio initialization failed ({})\n", Pa_GetErrorText(err)));
        return;
    }
    m_portAudioInitialized.store(true);
    // Pick the output device now so updateAudioOutput() can detect changes against it.
    m_audioOutputParameters.device = GetChosenApplicationAudioDevice();
}

bool DirectShowLayer::openAudioStreamLocked(int sampleRate, int channels) {
    // Retry backoff: don't hammer a failing device on every audio sample.
    const auto now = std::chrono::steady_clock::now();
    if (m_audioOpenLastFailed != std::chrono::steady_clock::time_point{} && now - m_audioOpenLastFailed < kAudioRetryBackoff)
        return false;

    // Determine output channel count (mirrors OmtLayer).
    int outChannels = channels;
    if (AudioSettings::portAudioMixInputToOutput()) {
        const PaDeviceInfo* devInfo = Pa_GetDeviceInfo(m_audioOutputParameters.device);
        if (devInfo)
            outChannels = std::min(AudioSettings::portAudioOutputChannels(), static_cast<int>(devInfo->maxOutputChannels));
    }
    m_audioOutputChannels.store(outChannels);

    m_audioOutputParameters.channelCount = outChannels;
    m_audioOutputParameters.sampleFormat = paFloat32;
    const PaDeviceInfo* devInfo = Pa_GetDeviceInfo(m_audioOutputParameters.device);
    if (devInfo)
        m_audioOutputParameters.suggestedLatency = devInfo->defaultLowOutputLatency;
    else
        m_audioOutputParameters.suggestedLatency = 0.05;
    m_audioOutputParameters.hostApiSpecificStreamInfo = nullptr;

    m_audioError = Pa_OpenStream(&m_audioStream, nullptr, &m_audioOutputParameters, sampleRate,
                                 paFramesPerBufferUnspecified, paClipOff, audioOutputCallback, this);
    if (m_audioError != paNoError) {
        sgct::Log::Error(std::format("DirectShowLayer: failed to open PortAudio stream ({})\n", Pa_GetErrorText(m_audioError)));
        m_audioOpenLastFailed = now;
        return false;
    }

    m_paSampleRate = sampleRate;
    m_audioSourceChannels = channels;
    m_audioStreamOpen.store(true);
    start(); // mirrors NdiLayer/OmtLayer: open + start together
    sgct::Log::Info(std::format("DirectShowLayer: audio output opened ({} Hz, {} -> {} ch)\n", sampleRate, channels, outChannels));
    return m_audioStreamStarted.load();
}

void DirectShowLayer::closeAudioStreamLocked() {
    if (m_audioStream) {
        if (m_audioStreamStarted.load()) {
            const PaError err = Pa_StopStream(m_audioStream);
            if (err == paNoError || err == paStreamIsStopped)
                m_audioStreamStarted.store(false);
            else
                Pa_AbortStream(m_audioStream); // mirrors NdiLayer::cleanup()
        }
        if (m_audioStreamOpen.load()) {
            const PaError err = Pa_CloseStream(m_audioStream);
            if (err == paNoError)
                m_audioStreamOpen.store(false);
        }
    }
    m_audioStream = nullptr;
    m_paSampleRate = 0;
}

void DirectShowLayer::closeAudioStream() {
    std::lock_guard<std::mutex> lock(m_audioStreamMutex);
    closeAudioStreamLocked();
}

// ---------------------------------------------------------------------------
// Low-latency WASAPI input for explicit capture microphones
// ---------------------------------------------------------------------------

bool DirectShowLayer::resolveCaptureDevices(std::string& videoDevice, std::string& audioDevice) const {
    // This layer may be created from a predefined setup: each machine resolves its own local
    // capture devices for the entry (by title) in its data/predefined-directshows.json. Entry not
    // found in the local file: fall back to the synced device pair. Render thread only.
    videoDevice = m_captureVideoDevice;
    audioDevice = m_captureAudioDevice;
    if (!m_presetKey.empty()) {
        std::string resolvedVideo, resolvedAudio;
        if (DirectShowPathResolver::instance().resolve(m_presetKey, isMaster(), resolvedVideo, resolvedAudio)) {
            videoDevice = resolvedVideo;
            audioDevice = resolvedAudio;
            return true;
        }
    }
    return false;
}

bool DirectShowLayer::openAudioInput(const std::string& deviceName) {
    // Render thread only. Opens a small-buffer WASAPI input stream for an explicit microphone so
    // audio arrives in ~10ms packets instead of the WDM filter's ~500ms blocks. Returns false when
    // PortAudio is unavailable or the device cannot be opened - the caller falls back to
    // buildAudioPath().
    if (!m_portAudioInitialized.load() || deviceName.empty() || m_audioInputStalled)
        return false;

    std::lock_guard<std::mutex> lock(m_audioStreamMutex);
    if (m_audioInputOpen.load()) {
        return m_audioInputDevice == deviceName; // already capturing from this microphone
    }
    closeAudioInputLocked(); // switching microphones - stop any previous input first

    // Case-insensitive containment in either direction.
    const auto ciContains = [](const std::string& haystack, const std::string& needle) {
        if (needle.empty() || needle.size() > haystack.size())
            return false;
        for (size_t off = 0; off + needle.size() <= haystack.size(); ++off) {
            bool same = true;
            for (size_t k = 0; k < needle.size(); ++k) {
                if (std::tolower(static_cast<unsigned char>(haystack[off + k])) != std::tolower(static_cast<unsigned char>(needle[k]))) {
                    same = false;
                    break;
                }
            }
            if (same)
                return true;
        }
        return false;
    };

    const int deviceCount = Pa_GetDeviceCount();
    for (int i = 0; i < deviceCount; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info || info->maxInputChannels <= 0)
            continue;

        // The preset stores the Windows endpoint name, which PortAudio reports verbatim; the
        // containment check tolerates small naming differences.
        const bool match = (deviceName == info->name) || ciContains(info->name, deviceName) || ciContains(deviceName, info->name);
        if (!match)
            continue;

        const int rate = static_cast<int>(info->defaultSampleRate); // native rate - no resampling needed
        const int channels = (info->maxInputChannels >= 2) ? 2 : 1;

        PaStreamParameters input{};
        input.device = i;
        input.channelCount = channels;
        input.sampleFormat = paFloat32; // interleaved - matches AudioRing::push()
        input.suggestedLatency = info->defaultLowInputLatency;
        input.hostApiSpecificStreamInfo = nullptr;

        // 256 frames (~5ms) per callback; stream flags are 0 (this PortAudio version predates paDefault/paNoFlag).
        PaError err = Pa_OpenStream(&m_audioInputStream, &input, nullptr, rate, 256, 0, audioInputCallback, this);
        if (err != paNoError) {
            sgct::Log::Error(std::format("DirectShowLayer: failed to open WASAPI input '{}' ({})\n", info->name, Pa_GetErrorText(err)));
            m_audioInputStream = nullptr;
            return false;
        }
        err = Pa_StartStream(m_audioInputStream);
        if (err != paNoError) {
            sgct::Log::Error(std::format("DirectShowLayer: failed to start WASAPI input '{}' ({})\n", info->name, Pa_GetErrorText(err)));
            Pa_CloseStream(m_audioInputStream);
            m_audioInputStream = nullptr;
            return false;
        }

        m_audioInputDevice = deviceName;
        m_audioInputRate.store(rate);
        m_audioInputChannels.store(channels);
        // Start the watchdog clock now: if no packet arrives within kAudioInputStallTimeout,
        // update() replaces this stream with the DirectShow WDM path.
        m_audioInputLastSampleMs.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now().time_since_epoch()).count());
        m_audioInputOpen.store(true);
        sgct::Log::Info(std::format("DirectShowLayer: audio via WASAPI input '{}' ({} Hz, {} ch, low latency)\n", info->name, rate, channels));

        // Open the output stream right away so audio starts without waiting for a first sample.
        // A failure here is retried from update() with backoff.
        openAudioStreamLocked(rate, channels);
        return true;
    }

    sgct::Log::Error(std::format("DirectShowLayer: no PortAudio input device matching '{}'\n", deviceName));
    return false;
}

void DirectShowLayer::closeAudioInputLocked() {
    if (m_audioInputStream) {
        const PaError err = Pa_StopStream(m_audioInputStream); // no-op when already stopped
        if (err != paNoError && err != paStreamIsStopped)
            Pa_AbortStream(m_audioInputStream); // mirrors closeAudioStreamLocked()
        Pa_CloseStream(m_audioInputStream);
        m_audioInputStream = nullptr;
    }
    m_audioInputOpen.store(false);
    m_audioInputRate.store(0);
    m_audioInputChannels.store(0);
}

void DirectShowLayer::closeAudioInput() {
    std::lock_guard<std::mutex> lock(m_audioStreamMutex);
    closeAudioInputLocked();
}

int DirectShowLayer::audioInputCallback(const void* inputBuffer, void*, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void* userData) {
    auto* layer = static_cast<DirectShowLayer*>(userData);
    if (!layer || !inputBuffer || framesPerBuffer == 0)
        return paContinue;
    if (!layer->m_receiveAudio.load())
        return paContinue; // audio disabled - drop samples until the stream is closed

    const int channels = layer->m_audioInputChannels.load();
    const int rate = layer->m_audioInputRate.load();
    if (channels <= 0 || rate <= 0)
        return paContinue;

    // AudioRing is internally synchronized; setFormat() is a no-op while the layout matches. The
    // output stream is opened from the render thread (openAudioInput/update), never here - PortAudio
    // callbacks must not call back into the API. Push only once it plays out, mirroring
    // handleAudioSample(), so stale audio never accumulates in the ring at startup.
    if (!layer->m_audioStreamOpen.load() || !layer->m_audioStreamStarted.load()) {
        layer->m_audioInputLastSampleMs.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                  std::chrono::steady_clock::now().time_since_epoch()).count(),
                                              std::memory_order_relaxed); // still feeds the stall watchdog in update()
        return paContinue;
    }
    layer->m_audioRing.setFormat(channels, rate);
    layer->m_audioRing.push(static_cast<const float*>(inputBuffer), framesPerBuffer);
    layer->m_audioInputLastSampleMs.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                                              std::chrono::steady_clock::now().time_since_epoch()).count(),
                                          std::memory_order_relaxed); // feeds the stall watchdog in update()
    return paContinue;
}

PaDeviceIndex DirectShowLayer::GetChosenApplicationAudioDevice() {
    PaDeviceIndex choseDeviceIdx = Pa_GetDefaultOutputDevice(); /* default output device */
    if (choseDeviceIdx == paNoDevice) {
        sgct::Log::Error("DirectShowLayer Error: No default audio output device.\n");
    }
    if (AudioSettings::portAudioCustomOutput()) {
        if (!AudioSettings::portAudioOutputDevice().isEmpty()
            && !AudioSettings::portAudioOutputApi().isEmpty()) {
            int numDevices = Pa_GetDeviceCount();
            if (numDevices < 0) {
                return choseDeviceIdx;
            }
            const PaDeviceInfo* deviceInfo;
            const PaHostApiInfo* apiInfo;
            sgct::Log::Info(std::format("DirectShowLayer: Trying to find audio device named \"{}\" using the \"{}\" api.",
                                        AudioSettings::portAudioOutputDevice().toStdString(),
                                        AudioSettings::portAudioOutputApi().toStdString()));
            bool foundDevice = false;
            for (int i = 0; i < numDevices; ++i) {
                deviceInfo = Pa_GetDeviceInfo(i);
                apiInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
                if (deviceInfo->maxOutputChannels > 1) {
                    const QString deviceName = QString::fromUtf8(deviceInfo->name);
                    const QString apiName = QString::fromUtf8(apiInfo->name);
                    if (deviceName == AudioSettings::portAudioOutputDevice()
                        && apiName == AudioSettings::portAudioOutputApi()) {
                        choseDeviceIdx = i;
                        foundDevice = true;
                        sgct::Log::Info("DirectShowLayer: Found desired audio device.\n");
                    }
                }
            }
            if (!foundDevice) {
                sgct::Log::Info("DirectShowLayer: Did not find desired audio device. Sticking with default device.\n");
            }
        }
    }
    return choseDeviceIdx;
}

int DirectShowLayer::audioOutputCallback(const void* /*inputBuffer*/, void* outputBuffer,
                                         unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo*,
                                         PaStreamCallbackFlags, void* userData) {
    auto* layer = static_cast<DirectShowLayer*>(userData);
    float* out = static_cast<float*>(outputBuffer);
    if (!layer || !out)
        return paContinue;

    // Drain the ring straight into the output buffer (no allocation on this thread).
    const int outChannels = layer->m_audioOutputChannels.load();
    // Mute zeroes the per-frame volume so no audio is written to the PortAudio stream.
    const float vol = layer->m_volumeMute.load() ? 0.f : layer->m_audioVolume.load();
    const size_t got = layer->m_audioRing.pop(out, outChannels, static_cast<size_t>(framesPerBuffer),
                                              vol);
    if (got < static_cast<size_t>(framesPerBuffer)) {
        // Underrun: silence the rest of the buffer.
        std::fill_n(out + got * outChannels, (static_cast<size_t>(framesPerBuffer) - got) * outChannels, 0.0f);
    }

    // Report the peak of this frame for the audio level meter in the LayerView, but only
    // while the meter is enabled so the per-sample scan stays out of the hot path.
    if (layer->audioLevelsEnabled()) {
        float maxAbs = 0.f;
        const size_t totalSamples = static_cast<size_t>(framesPerBuffer) * static_cast<size_t>(outChannels);
        for (size_t i = 0; i < totalSamples; ++i) {
            float v = out[i];
            if (v < 0.f) v = -v;
            if (v > maxAbs) maxAbs = v;
        }
        layer->reportAudioLevel(maxAbs);
    }

    return paContinue;
}

void DirectShowLayer::start() {
    if (!m_audioStreamStarted.load() && isAudioEnabled() && m_audioStream && m_audioStreamOpen.load()) {
        setVolume(m_volume);
        m_audioError = Pa_StartStream(m_audioStream);
        if (m_audioError == paNoError) {
            m_audioStreamStarted.store(true);
        }
    }
}

void DirectShowLayer::stop() {
    if (isAudioEnabled() && m_audioStream && m_audioStreamOpen.load() && m_audioStreamStarted.load()) {
        m_audioError = Pa_StopStream(m_audioStream);
        if (m_audioError == paNoError) {
            m_audioStreamStarted.store(false);
        }
    }
}

bool DirectShowLayer::hasAudio() const {
    return (isAudioEnabled() || (isMaster() && AudioSettings::enableAudioOnNodes()));
}

bool DirectShowLayer::isAudioEnabled() const {
    return m_isAudioEnabled;
}

void DirectShowLayer::enableAudio(bool enabled) {
    m_isAudioEnabled = enabled;
}

void DirectShowLayer::updateAudioOutput() {
    if (isMaster()) {
        if (!m_isAudioEnabled && AudioSettings::enableAudioOnMaster()) {
            enableAudio(true);
        } else if (m_isAudioEnabled && !AudioSettings::enableAudioOnMaster()) {
            enableAudio(false);
        }
    }
    if (!isAudioEnabled() || !m_portAudioInitialized.load())
        return;

    // See if device has changed.
    const PaDeviceIndex newDeviceIdx = GetChosenApplicationAudioDevice();

    std::lock_guard<std::mutex> lock(m_audioStreamMutex); // the streaming thread may open/close concurrently
    const PaDeviceIndex currentDeviceIdx = m_audioOutputParameters.device;

    int channelCount = (m_audioSourceChannels > 0) ? m_audioSourceChannels : 2;
    if (AudioSettings::portAudioMixInputToOutput()) {
        const PaDeviceInfo* devInfo = Pa_GetDeviceInfo(newDeviceIdx);
        if (devInfo)
            channelCount = std::min(AudioSettings::portAudioOutputChannels(), static_cast<int>(devInfo->maxOutputChannels));
    }

    bool restartStream = false;
    if (newDeviceIdx != currentDeviceIdx) {
        restartStream = true;
    } else if (m_audioStreamOpen.load() && m_audioOutputParameters.channelCount != channelCount) {
        restartStream = true;
    }
    if (!restartStream)
        return;

    // Close the stream to restart it with the new device/channel count.
    const bool wasStarted = m_audioStreamStarted.load();
    const int savedRate = m_paSampleRate;
    const int savedChannels = m_audioSourceChannels;
    closeAudioStreamLocked();

    // If the stream is closed, let's switch device.
    if (!m_audioStreamOpen.load()) {
        m_audioOutputParameters.device = newDeviceIdx;

        // Let's start again if started (same source format as before).
        if (wasStarted && savedRate > 0) {
            openAudioStreamLocked(savedRate, savedChannels);
        }
    }
}

void DirectShowLayer::setVolume(int v, bool storeLevel) {
    if (storeLevel) {
        m_volume = v;
    }

    // The PortAudio callback reads the volume atomically - no stream lock needed.
    m_audioVolume.store(static_cast<float>(v) / 100.0f);

    if (isMaster() && AudioSettings::enableAudioOnNodes())
        setNeedSync();
}

void DirectShowLayer::setVolumeMute(bool v) {
    if (m_volumeMute.load() == v)
        return;

    // The PortAudio callback reads the flag atomically - no stream lock needed.
    m_volumeMute.store(v);
}

#endif // _WIN32
