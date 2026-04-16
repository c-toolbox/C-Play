/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "qrcommandprocessor.h"
#include "qrcodereader.h"
#include <sgct/sgct.h>
#include <algorithm>
#include <sstream>

QRCommandProcessor::QRCommandProcessor()
    : m_reader(new QRCodeReader)
{
}

QRCommandProcessor::~QRCommandProcessor() {
    delete m_reader;
}

void QRCommandProcessor::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (!m_enabled) {
        m_operationsQueue.clear();
    }
}

bool QRCommandProcessor::isEnabled() const {
    return m_enabled;
}

void QRCommandProcessor::setDelimiter(char delim) {
    m_delimiter = delim;
}

void QRCommandProcessor::setCommandCallback(CommandCallback callback) {
    m_callback = std::move(callback);
}

bool QRCommandProcessor::processFrame(unsigned char* pixelData, unsigned int width, unsigned int height, int GLformat) {
    if (!m_enabled) {
        return false;
    }

    // Phase 1: Scan for QR codes
    std::vector<std::string> decodedResults = m_reader->scan(pixelData, width, height, GLformat);

    if (!decodedResults.empty()) {
        // QR codes detected: queue unique operations, signal caller to skip frame
        for (const auto& decoded : decodedResults) {
            if (std::find(m_operationsQueue.begin(), m_operationsQueue.end(), decoded) == m_operationsQueue.end()) {
                sgct::Log::Info(std::format("QRCommandProcessor: Queued command ({} chars): {}", decoded.size(), decoded));
                m_operationsQueue.push_back(decoded);
            }
        }
        return true; // Skip this frame (control frame)
    }

    // Phase 2: No QR codes detected - execute queued commands if any
    if (!m_operationsQueue.empty()) {
        if (m_callback) {
            for (const auto& raw : m_operationsQueue) {
                QRCommand cmd = parseCommand(raw);
                if (!cmd.target.empty() && !cmd.actions.empty()) {
                    sgct::Log::Info(std::format("QRCommandProcessor: Executing command for target: {}", cmd.target));
                    m_callback(cmd);
                }
            }
        }
        m_operationsQueue.clear();
    }

    return false; // Proceed with normal frame processing
}

void QRCommandProcessor::clearQueue() {
    m_operationsQueue.clear();
}

bool QRCommandProcessor::hasPendingCommands() const {
    return !m_operationsQueue.empty();
}

const std::vector<std::string>& QRCommandProcessor::pendingCommands() const {
    return m_operationsQueue;
}

QRCommand QRCommandProcessor::parseCommand(const std::string& raw) const {
    QRCommand cmd;
    std::stringstream ss(raw);
    std::string token;
    bool first = true;
    while (std::getline(ss, token, m_delimiter)) {
        if (token.empty()) continue;
        if (first) {
            cmd.target = token;
            first = false;
        }
        else {
            cmd.actions.push_back(token);
        }
    }
    return cmd;
}
