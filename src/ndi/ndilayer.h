#ifndef NDILAYER_H
#define NDILAYER_H

#include <layers/baselayer.h>
#include <sgct/opengl.h>
#include <ndi/ofxNDI/ofxNDIreceive.h>

class NdiLayer : public BaseLayer
{
public:
    NdiLayer();
    ~NdiLayer();

	bool ReceiveImage();

	// Bunch of code from ofxNDIreciever

	// Create an NDI finder to find existing senders
	void CreateFinder();

	// Release an NDI finder that has been created
	void ReleaseFinder();

	// Find all current NDI senders
	int FindSenders();

	// Find all current senders and refresh sender list
	// If no timeout specified, return the sources that exist right now
	// For a timeout, wait for that timeout and return the sources that exist then
	// If that fails, return NULL
	int RefreshSenders(uint32_t timeout);

	// Open a receiver ready to receive
	bool OpenReceiver();

	// Create a receiver
	// - index | index in the sender list to connect to
	//   -1 - connect to the selected sender
	//        if none selected connect to the first sender
	// Default NDI format is BGRA
	// Default application format is RGBA
	// Data is converted to RGBA during copy from the video frame buffer
	bool CreateReceiver(int index = -1);

	// Create a receiver with preferred colour format
	// - colorFormat | the preferred format
	// - index | index in the sender list to connect to
	//   -1 - connect to the selected sender
	//        if none selected connect to the first sender
	bool CreateReceiver(NDIlib_recv_color_format_e colorFormat, int index = -1);

	// Return whether a receiver has been created
	bool ReceiverCreated();

	// Return whether a receiver has connected to a sender
	bool ReceiverConnected();

	// Close receiver and release resources
	void ReleaseReceiver();

	// Set current sender index in the sender list
	bool SetSenderIndex(int index);

	// Current index in the sender list
	int GetSenderIndex();

	// The index of a sender name
	bool GetSenderIndex(char* sendername, int& index);

	// Set a sender name to receive from
	void SetSenderName(std::string sendername);

	// Name string of a sender index
	std::string GetSenderName(int index = -1);

	// Name characters of a sender index
	// For back-compatibility
	bool GetSenderName(char* sendername);
	bool GetSenderName(char* sendername, int index);
	bool GetSenderName(char* sendername, int maxsize, int index);

	// Sender width
	unsigned int GetSenderWidth();

	// Sender height
	unsigned int GetSenderHeight();

	// Sender frame rate
	float GetSenderFps();

	// Number of senders
	int GetSenderCount();

	// Return the list of senders
	std::vector<std::string> GetSenderList();

	// Received frame type
	NDIlib_frame_type_e GetFrameType();

	// Is the current frame MetaData ?
	// Use when ReceiveImage fails
	bool IsMetadata();

	// The current MetaData string
	std::string GetMetadataString();

	// The current video frame timestamp
	int64_t GetVideoTimestamp();

	// The current video frame timecode
	int64_t GetVideoTimecode();

	// Set NDI low banwidth option
	// Default false
	void SetLowBandwidth(bool bLow = true);

	// Set to receive Audio
	void SetAudio(bool bAudio = true);

	// Is the current frame Audio data ?
	// Use when ReceiveImage fails
	bool IsAudioFrame();

	// Number of audio channels
	int GetAudioChannels();

	// Number of audio samples
	int GetAudioSamples();

	// Audio sample rate
	int GetAudioSampleRate();

	// Get audio frame data pointer
	float* GetAudioData();

	// Return audio frame data
	void GetAudioData(float*& output, int& samplerate, int& samples, int& nChannels);

	// The NDI SDK version number
	std::string GetNDIversion();

	// Timed received frame rate
	int GetFps();

private:
	// Basic receiver functions
	ofxNDIreceive NDIreceiver;

	bool GetPixelData(GLuint TextureID, unsigned int width, unsigned int height);
	bool LoadTexturePixels(GLuint TextureID, unsigned int width, unsigned int height, unsigned char* data, int GLformat = GL_BGRA);
	void GenerateTexture(unsigned int& id, int width, int height);

	GLuint m_pbo[2]; // PBOs used for asynchronous pixel load
	int PboIndex = 0; // Index used for asynchronous pixel load
	int NextPboIndex = 0;

};

#endif // NDILAYER_H