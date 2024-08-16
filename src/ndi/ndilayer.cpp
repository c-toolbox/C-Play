#include "ndilayer.h"

NdiLayer::NdiLayer()
{
	setType(BaseLayer::LayerType::NDI);
}

NdiLayer::~NdiLayer()
{
}

// Receive ofTexture
bool NdiLayer::ReceiveImage()
{
	// Check for receiver creation
	if (!OpenReceiver()) {
		return false;
	}

	// Receive a pixel image first
	unsigned int width = (unsigned int)renderData.width;
	unsigned int height = (unsigned int)renderData.height;

	if (NDIreceiver.ReceiveImage(width, height)) {
		// Check for changed sender dimensions
		if (width != (unsigned int)renderData.width || height != (unsigned int)renderData.height) {
			if(renderData.width > 0)
				glDeleteTextures(1, &renderData.texId);

			GenerateTexture(renderData.texId, width, height);

			renderData.width = (int)width;
			renderData.height = (int)height;
		}
		// Get NDI pixel data from the video frame
		return GetPixelData(renderData.texId, width, height);
	}

	return false;
}

// Bunch of code from ofxNDIreciever

// Create a finder to look for a sources on the network
void NdiLayer::CreateFinder()
{
	NDIreceiver.CreateFinder();
}

// Release the current finder
void NdiLayer::ReleaseFinder()
{
	NDIreceiver.ReleaseFinder();
}

// Find all current NDI senders
int NdiLayer::FindSenders()
{
	return NDIreceiver.FindSenders();
}

// Refresh sender list with the current network snapshot
int NdiLayer::RefreshSenders(uint32_t timeout)
{
	return NDIreceiver.RefreshSenders(timeout);
}

// Create receiver if not initialized or a new sender has been selected
bool NdiLayer::OpenReceiver()
{
	if (NDIreceiver.OpenReceiver()) {
		// Initialize pbos for asynchronous pixel load
		if (!m_pbo[0]) {
			glGenBuffers(2, m_pbo);
			PboIndex = NextPboIndex = 0;
		}
		return true;
	}
	return false;
}

// Create a receiver
bool NdiLayer::CreateReceiver(int userindex)
{
	return NDIreceiver.CreateReceiver(userindex);
}

// Create a receiver with preferred colour format
bool NdiLayer::CreateReceiver(NDIlib_recv_color_format_e color_format, int userindex)
{
	return NDIreceiver.CreateReceiver(color_format, userindex);
}

// Return whether the receiver has been created
bool NdiLayer::ReceiverCreated()
{
	return NDIreceiver.ReceiverCreated();
}

// Return whether a receiver has connected to a sender
bool NdiLayer::ReceiverConnected()
{
	return NDIreceiver.ReceiverConnected();
}

// Close receiver and release resources
void NdiLayer::ReleaseReceiver()
{
	NDIreceiver.ReleaseReceiver();
}

// Set the sender list index variable
bool NdiLayer::SetSenderIndex(int index)
{
	return NDIreceiver.SetSenderIndex(index);
}

// Return the index of the current sender
int NdiLayer::GetSenderIndex()
{
	return NDIreceiver.GetSenderIndex();
}

// Return the index of a sender name
bool NdiLayer::GetSenderIndex(char* sendername, int& index)
{
	return NDIreceiver.GetSenderIndex(sendername, index);
}

// Return the number of senders
int NdiLayer::GetSenderCount()
{
	return NDIreceiver.GetSenderCount();
}

// Return the list of senders
std::vector<std::string> NdiLayer::GetSenderList()
{
	return NDIreceiver.GetSenderList();
}

// Set a sender name to receive from
void NdiLayer::SetSenderName(std::string sendername)
{
	NDIreceiver.SetSenderName(sendername);
}

// Return the name string of a sender index
std::string NdiLayer::GetSenderName(int userindex)
{
	return NDIreceiver.GetSenderName(userindex);
}

// Return the name characters of a sender index
// For back-compatibility only
// Char functions replaced with string versions
bool NdiLayer::GetSenderName(char* sendername)
{
	// Note : length of user name string is not known
	int index = -1;
	return GetSenderName(sendername, 128, index);
}

bool NdiLayer::GetSenderName(char* sendername, int index)
{
	// NOTE : length of user name string is not known
	return GetSenderName(sendername, 128, index);
}

bool NdiLayer::GetSenderName(char* sendername, int maxsize, int userindex)
{
	return NDIreceiver.GetSenderName(sendername, maxsize, userindex);
}

// Return the current sender width
unsigned int NdiLayer::GetSenderWidth() {
	return NDIreceiver.GetSenderWidth();
}

// Return the current sender height
unsigned int NdiLayer::GetSenderHeight() {
	return NDIreceiver.GetSenderHeight();
}

// Return current sender frame rate
float NdiLayer::GetSenderFps()
{
	return NDIreceiver.GetSenderFps();
}

// Return the received frame type.
// Note that "allow_video_fields" is currently set false when
// the receiver is created, so all video received will be progressive
NDIlib_frame_type_e NdiLayer::GetFrameType()
{
	return NDIreceiver.GetFrameType();
}

// Is the current frame MetaData ?
bool NdiLayer::IsMetadata()
{
	return NDIreceiver.IsMetadata();
}

// Return the current MetaData string
std::string NdiLayer::GetMetadataString()
{
	return NDIreceiver.GetMetadataString();
}

// Return the current video frame timestamp
int64_t NdiLayer::GetVideoTimestamp()
{
	return NDIreceiver.GetVideoTimestamp();
}

// Return the current video frame timecode
// UTC time since the Unix Epoch (1/1/1970 00:00) with 100 ns precision.
int64_t NdiLayer::GetVideoTimecode()
{
	return NDIreceiver.GetVideoTimecode();
}

//
// Bandwidth
//
// true
// NDIlib_recv_bandwidth_lowest will provide a medium quality stream that takes almost no bandwidth,
// this is normally of about 640 pixels in size on it is longest side and is a progressive video stream.
// false (default)
// NDIlib_recv_bandwidth_highest will result in the same stream that is being sent from the up-stream source
//
void NdiLayer::SetLowBandwidth(bool bLow)
{
	NDIreceiver.SetLowBandwidth(bLow);
}

// Set to receive Audio
void NdiLayer::SetAudio(bool bAudio)
{
	NDIreceiver.SetAudio(bAudio);
}

// Is the current frame Audio ?
bool NdiLayer::IsAudioFrame()
{
	return NDIreceiver.IsAudioFrame();
}

// Number of audio channels
int NdiLayer::GetAudioChannels()
{
	return NDIreceiver.GetAudioChannels();
}

// Number of audio samples
int NdiLayer::GetAudioSamples()
{
	return NDIreceiver.GetAudioSamples();
}

// Audio sample rate
int NdiLayer::GetAudioSampleRate()
{
	return NDIreceiver.GetAudioSampleRate();
}

// Get audio frame data pointer
float* NdiLayer::GetAudioData()
{
	return NDIreceiver.GetAudioData();
}

// Return audio frame data
// output - audio data pointer, samplerate, samples and channels
void NdiLayer::GetAudioData(float*& output, int& samplerate, int& samples, int& nChannels)
{
	NDIreceiver.GetAudioData(output, samplerate, samples, nChannels);
}

// Return the NDI dll version number
std::string NdiLayer::GetNDIversion()
{
	return NDIreceiver.GetNDIversion();
}

// Return the received frame rate
int NdiLayer::GetFps()
{
	return NDIreceiver.GetFps();
}

// Get NDI pixel data from the video frame to ofTexture 
bool NdiLayer::GetPixelData(GLuint TextureID, unsigned int width, unsigned int height)
{
	// Get the video frame buffer pointer
	unsigned char* videoData = NDIreceiver.GetVideoData();
	if (!videoData) {
		// Ensure the video buffer is freed
		NDIreceiver.FreeVideoData();
		return false;
	}

	// Get the NDI video frame pixel data into the texture
	switch (NDIreceiver.GetVideoType()) {
		// Note : the receiver is set up to prefer BGRA format by default
		// If set to prefer NDIlib_recv_color_format_fastest, YUV data is received.
		// YCbCr - Load texture with YUV data by way of PBO
	case NDIlib_FourCC_type_UYVY: // YCbCr using 4:2:2
		printf("GetPixelData - UYVY format not supported\n"); break;
	case NDIlib_FourCC_type_UYVA: // YCbCr using 4:2:2:4
		printf("GetPixelData - UYVA format not supported\n"); break;
	case NDIlib_FourCC_type_P216: // YCbCr using 4:2:2 in 16bpp
		printf("GetPixelData - P216 format not supported\n"); break;
	case NDIlib_FourCC_type_PA16: // YCbCr using 4:2:2:4 in 16bpp
		printf("GetPixelData - PA16 format not supported\n");
		break;
	case NDIlib_FourCC_type_RGBX: // RGBX
	case NDIlib_FourCC_type_RGBA: // RGBA
			LoadTexturePixels(TextureID, width, height, (unsigned char*)videoData, GL_RGBA);
	case NDIlib_FourCC_type_BGRX: // BGRX
	case NDIlib_FourCC_type_BGRA: // BGRA
	default: // BGRA
			LoadTexturePixels(TextureID, width, height, (unsigned char*)videoData, GL_BGRA);
	} // end switch received format

	// Free the NDI video buffer
	NDIreceiver.FreeVideoData();

	return true;

}

// Streaming texture pixel load
// From : http://www.songho.ca/opengl/gl_pbo.html
// Approximately 20% faster than using glTexSubImage2D alone
// GLformat can be default GL_BGRA or GL_RGBA
bool NdiLayer::LoadTexturePixels(GLuint TextureID, unsigned int width, unsigned int height, unsigned char* data, int GLformat)
{
	void* pboMemory = NULL;

	PboIndex = (PboIndex + 1) % 2;
	NextPboIndex = (PboIndex + 1) % 2;

	// Bind the texture and PBO
	glBindTexture(GL_TEXTURE_2D, TextureID);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo[PboIndex]);

	// Copy pixels from PBO to the texture - use offset instead of pointer.
	// glTexSubImage2D redefines a contiguous subregion of an existing
	// two-dimensional texture image. NULL data pointer reserves space.
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GLformat, GL_UNSIGNED_BYTE, 0);

	// Bind PBO to update the texture
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo[NextPboIndex]);

	// Call glBufferData() with a NULL pointer to clear the PBO data and avoid a stall.
	glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * 4, 0, GL_STREAM_DRAW);

	// Map the buffer object into client's memory
	pboMemory = (void*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
	// Update the mapped buffer directly
	if (pboMemory) {
		// RGBA pixel data
		// Use sse2 if the width is divisible by 16
		ofxNDIutils::CopyImage(data, (unsigned char*)pboMemory, width, height);
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release the mapped buffer
	}
	else {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		return false;
	}

	// Release PBOs
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	return true;

}

void NdiLayer::GenerateTexture(unsigned int& id, int width, int height) {
	glGenTextures(1, &id);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glBindTexture(GL_TEXTURE_2D, id);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);

	// Disable mipmaps
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}


