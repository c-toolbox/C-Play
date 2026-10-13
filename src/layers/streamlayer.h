#ifndef STREAMLAYER_H
#define STREAMLAYER_H

#include <layers/videolayer.h>

class StreamLayer : public VideoLayer {
public:
    StreamLayer(gl_adress_func_v1 opa,
        bool allowDirectRendering = false,
        bool loggingOn = false,
        std::string logLevel = "info",
        MpvLayer::onFileLoadedCallback flc = nullptr);
    ~StreamLayer();

    void initialize();
    void update(bool updateRendering = true) override;
    void updateFrame();
    bool ready() const;

    // Stable key identifying the predefined stream entry this layer was created from (the entry's title). Empty for custom stream paths. Each machine resolves its own local path from this key via its local predefined-streams.json; an empty resolved path means no stream on that machine.
    std::string streamKey() const;
    void setStreamKey(const std::string& key);

    // The actual local media path for this layer on this machine (see streamKey()).
    std::string effectiveFilePath() const override;

    void encodeTypeCore(std::vector<std::byte>& data) override;
    void decodeTypeCore(const std::vector<std::byte>& data, unsigned int& pos) override;

    void encodeTypeAlways(std::vector<std::byte>& data);
    void decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos);

    void encodeTypeProperties(std::vector<std::byte>& data);
    void decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos);

private:
    bool refreshMasterPlaceholder();

    // Stable key of the predefined stream entry this layer was created from (entry title); empty for custom paths.
    std::string m_streamKey;
};

#endif // STREAMLAYER_H