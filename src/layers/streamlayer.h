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

    void encodeTypeAlways(std::vector<std::byte>& data);
    void decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos);

    void encodeTypeProperties(std::vector<std::byte>& data);
    void decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos);

    bool isQRCodeDetectionEnabled() const override;
    void setQRCodeDetectionEnabled(bool enabled) override;

    int textureDivisionMode() const override;
    void setTextureDivisionMode(int mode) override;

    int textureDivisionGrid() const override;
    void setTextureDivisionGrid(int grid) override;

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

    // Copy the current renderData texture to the backup texture.
    void copyToBackupTexture(unsigned int srcTexId, int width, int height);

    // QR command processing
    QRCommandProcessor* m_qrProcessor = nullptr;

    // QR operation handler (sublayers, config, SetActive/Clear/Freeze)
    QROperationHandler* m_qrOpHandler = nullptr;

    // Texture division handler (alternative to QR operation)
    class DivideTextureHandler* m_divideTexHandler = nullptr;
    int m_textureDivisionMode = 0;  // 0=None, 1=ImPres(QR), 2=Division
    int m_textureDivisionGrid = 0;  // grid index

    bool m_qrCodeDetectionEnabled_Dec = false;
    bool m_typePropertiesDecoded = false;

    // Readback buffer for QR code scanning from FBO texture
    unsigned char* m_readbackBuffer = nullptr;
    size_t m_readbackBufferSize = 0;

    // Backup texture: holds the last clean frame (no QR code).
    // When a QR code is detected, renderData.texId is swapped to this backup
    // so the displayed frame does not show the control frame.
    unsigned int m_backupTexId = 0;
    int m_backupTexWidth = 0;
    int m_backupTexHeight = 0;
};

#endif // STREAMLAYER_H