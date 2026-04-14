#ifndef STREAMLAYER_H
#define STREAMLAYER_H

#include <layers/videolayer.h>

class QRCommandProcessor;
class QROperationHandler;
class QROperationConfig;
struct QRCommand;

class StreamLayer : public VideoLayer {
public:
    StreamLayer(gl_adress_func_v1 opa,
        bool allowDirectRendering = false,
        bool loggingOn = false,
        std::string logLevel = "info",
        MpvLayer::onFileLoadedCallback flc = nullptr);
    ~StreamLayer();

    void initialize();
    void updateFrame();
    bool ready() const;

    void encodeTypeProperties(std::vector<std::byte>& data);
    void decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos);

    bool isQRCodeDetectionEnabled() const override;
    void setQRCodeDetectionEnabled(bool enabled) override;

    // Load QR operation plane configuration from a JSON file.
    bool loadQROperationConfig(const std::string& filePath);

    // Access the QR operation configuration.
    const QROperationConfig* qrOperationConfig() const;

    // Get the name of the currently active (live) plane, or empty if none.
    std::string activePlaneName() const;

    bool hasSubLayers() const override;
    std::vector<std::shared_ptr<BaseLayer>>& getSubLayers() const override;

private:
    bool FindCodes(unsigned int texId, unsigned int width, unsigned int height);
    void onQRCommand(const QRCommand& command);

    // QR command processing
    QRCommandProcessor* m_qrProcessor = nullptr;

    // QR operation handler (sublayers, config, SetActive/Clear/Freeze)
    QROperationHandler* m_qrOpHandler = nullptr;

    bool m_qrCodeDetectionEnabled_Dec = false;
    bool m_typePropertiesDecoded = false;

    // Readback buffer for QR code scanning from FBO texture
    unsigned char* m_readbackBuffer = nullptr;
    size_t m_readbackBufferSize = 0;
};

#endif // STREAMLAYER_H