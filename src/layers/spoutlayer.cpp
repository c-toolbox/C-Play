/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "spoutlayer.h"
#include <fmt/core.h>
#include <sgct/opengl.h>
#include <sgct/sgct.h>

SpoutFinder* SpoutFinder::_instance = nullptr;

SpoutFinder::SpoutFinder() {
	m_spoutlibrary = GetSpout();
	if (!m_spoutlibrary) {
		sgct::Log::Error("SpoutLayer Error: Spout library load failed.\n");
	}
}

SpoutFinder::~SpoutFinder() {
	if (!m_spoutlibrary) {
		// Release the library on exit
		m_spoutlibrary->Release();
	}
	delete _instance;
	_instance = nullptr;
}

SpoutFinder& SpoutFinder::instance() {
	if (!_instance) {
		_instance = new SpoutFinder();
	}
	return *_instance;
}

int SpoutFinder::senderCount() {
	return m_spoutlibrary->GetSenderCount();
}

std::vector<std::string> SpoutFinder::getSendersList() {
	std::vector<std::string> sendersList;
	int nSenders = m_spoutlibrary->GetSenderCount();
	if (nSenders > 0) {
		char SenderName[256];
		for (int i = 0; i < nSenders; i++) {
			m_spoutlibrary->GetSender(i, SenderName);
			sendersList.push_back(SenderName);
		}
	}
	return sendersList;
}

bool SpoutFinder::senderExists(std::string senderName) {
	return m_spoutlibrary->FindSenderName(senderName.c_str());
}

std::string SpoutFinder::getSpoutVersionString() {
	return m_spoutlibrary->GetSDKversion();
}

SpoutLayer::SpoutLayer() {
    setType(BaseLayer::LayerType::SPOUT);

	m_receiver = GetSpout();
	if (!m_receiver) {
		sgct::Log::Error("SpoutLayer Error: Spout library load failed.\n");
	}
}

SpoutLayer::~SpoutLayer() {
	if (m_receiver) {
		// Release the receiver
		m_receiver->ReleaseReceiver();
		// Release the library on exit
		m_receiver->Release();
	}
}

void SpoutLayer::initialize() {
    m_hasInitialized = true;

	// Specify the sender to connect to.
	// The application will not connect to any other unless the user selects one.
	// If that sender closes, the application will wait for the nominated sender to open.
	m_receiver->SetReceiverName(filepath().c_str());

	// Set as active
	//m_receiver->SetActiveSender(filepath().c_str());
	// Change to the active sender
	//m_receiver->SetReceiverName();
}

void SpoutLayer::update(bool updateRendering) {
	//
	// ReceiveTexture or ReceiveImage connect to and receive from a sender
	// Optionally include the ID of an fbo if one is currently bound
	//
	// For successful receive, sender details can be retrieved with
	//		const char * GetSenderName();
	//		unsigned int GetSenderWidth();
	//		unsigned int GetSenderHeight();
	//		DWORD GetSenderFormat();
	//		double GetSenderFps();
	//		long GetSenderFrame();
	//
	// If receive fails, the sender has closed
	// Connection can be tested at any time with 
	//		bool IsConnected();
	//

	// Let's recieve image or audio
	if (ready() && updateRendering) {
		unsigned int width = m_receiver->GetSenderWidth();
		unsigned int height = m_receiver->GetSenderHeight();
		// If IsUpdated() returns true, the sender size has changed
		// and the receiving texture or pixel buffer must be re-sized.
		if (m_receiver->IsUpdated() 
			|| width != (unsigned int)renderData.width 
			|| height != (unsigned int)renderData.height) {
			width = m_receiver->GetSenderWidth();
			height = m_receiver->GetSenderHeight();
			
			// Check for changed sender dimensions
			if (width != (unsigned int)renderData.width || height != (unsigned int)renderData.height) {
				if (renderData.width > 0)
					glDeleteTextures(1, &renderData.texId);

				GenerateTexture(renderData.texId, width, height);

				renderData.width = (int)width;
				renderData.height = (int)height;
			}
		}

		// Receive texture
		if(m_receiver->IsConnected())
			m_receiver->ReceiveTexture(renderData.texId, GL_TEXTURE_2D);
	}
}

bool SpoutLayer::ready() const {
	int nSenders = m_receiver->GetSenderCount();

	if (nSenders < 1)
		return false;

	if (m_receiver->FindSenderName(filepath().c_str())) {
		if (strcmp(m_receiver->GetSenderName(), filepath().c_str()) != 0) {
			m_receiver->SetReceiverName(filepath().c_str());
		}
		return true;
	}

    return false;
}

void SpoutLayer::GenerateTexture(unsigned int& id, int width, int height) {
    glGenTextures(1, &id);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Disable mipmaps
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}