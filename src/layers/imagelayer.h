#ifndef IMAGELAYER_H
#define IMAGELAYER_H

#include <layers/baselayer.h>
#include <sgct/sgct.h>

class ImageLayer : public BaseLayer {
public:
    struct ImageData {
        std::string filename = "";
        std::string identifier = "";
        sgct::Image img;
        std::unique_ptr<std::thread> trd;
        std::atomic_bool threadRunning = false;
        std::atomic_bool imageDone = false;
        std::atomic_bool uploadDone = false;
        std::atomic_bool threadDone = false;
    };

    ImageLayer(std::string identifier);
    ~ImageLayer();

    void initialize();
    void update(bool updateRendering = true);
    bool ready() const;

    bool processImageUpload(std::string filename, bool forceUpdate);
    std::string loadedFile();

    bool fileIsImage(std::string &filePath);

private:
    ImageData imageData;

    void handleAsyncImageUpload();
};

#endif // IMAGELAYER_H