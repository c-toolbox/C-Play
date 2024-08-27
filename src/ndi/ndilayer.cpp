#include "ndilayer.h"

NdiLayer::NdiLayer()
{
	setType(BaseLayer::LayerType::NDI);
	NDIreceiver.ResetFps(30.0);

	// =======================================
	NDIreceiver.SetAudio(false); // Set to receive no audio

	// =======================================
	// Set to prefer BGRA
	NDIreceiver.SetFormat(NDIlib_recv_color_format_BGRX_BGRA);

	OpenReceiver();
}

NdiLayer::~NdiLayer()
{
	if (m_pbo[0]) {
		glDeleteBuffers(2, m_pbo);
	}

	if (renderData.texId > 0)
		glDeleteTextures(1, &renderData.texId);
}

void NdiLayer::update()
{
	ReceiveImage();
}

bool NdiLayer::ready()
{
	return m_isReady;
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

			NDIreceiver.SetSenderName(filepath());
			renderData.width = (int)width;
			renderData.height = (int)height;
		}
		// Get NDI pixel data from the video frame
		return GetPixelData(renderData.texId, width, height);
	}

	return false;
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
			LoadTexturePixels(TextureID, width, height, videoData, GL_RGBA);
			break;
	case NDIlib_FourCC_type_BGRX: // BGRX
	case NDIlib_FourCC_type_BGRA: // BGRA
	default: // BGRA
			LoadTexturePixels(TextureID, width, height, videoData, GL_BGRA);
			break;
	} // end switch received format

	// Free the NDI video buffer
	NDIreceiver.FreeVideoData();

	m_isReady = true;

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
		ofxNDIutils::CopyImage(data, (unsigned char*)pboMemory, width, height, true);
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
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		break;
	case NDIlib_FourCC_type_BGRX: // BGRX
	case NDIlib_FourCC_type_BGRA: // BGRA
	default: // BGRA
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
		break;
	} // end switch received format

	// Disable mipmaps
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}


