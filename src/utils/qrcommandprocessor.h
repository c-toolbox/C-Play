/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef QRCOMMANDPROCESSOR_H
#define QRCOMMANDPROCESSOR_H

#include <string>
#include <vector>
#include <functional>

class QRCodeReader;

// A parsed QR command: target name and list of action tokens.
// QR code text format: "<target>;<action1>;<action2>;..."
struct QRCommand {
    std::string target;
    std::vector<std::string> actions;
};

// Two-phase QR command processor inspired by the ImPres scheme.
//
// Phase 1 (QR codes detected in frame):
//   - Unique command strings are queued.
//   - processFrame() returns false, signalling the caller to skip using this
//     frame for display (it is a "control frame").
//
// Phase 2 (No QR codes detected, queue non-empty):
//   - All queued commands are parsed and dispatched via the registered callback.
//   - The queue is cleared.
//   - processFrame() returns true, signalling the caller to proceed with
//     normal frame processing.
//
// When no QR codes are detected and the queue is already empty,
// processFrame() returns true immediately (normal frame).
//
class QRCommandProcessor {
public:
    // Callback signature for command execution.
    // Called once per parsed command when the queue is flushed (phase 2).
    using CommandCallback = std::function<void(const QRCommand& command)>;

    QRCommandProcessor();
    ~QRCommandProcessor();

    // Enable or disable the processor. When disabled processFrame() always returns true.
    void setEnabled(bool enabled);
    bool isEnabled() const;

    // Set the delimiter used to split QR text into target + actions. Default is ';'.
    void setDelimiter(char delim);

    // Register the callback that will be invoked for each command during phase 2.
    void setCommandCallback(CommandCallback callback);

    // Process a video frame. Returns:
    //   true  - caller should proceed with normal frame handling (upload texture, etc.)
    //   false - caller should skip this frame (QR codes detected, queuing commands)
    //
    // pixelData, width, height, GLformat: image data (GL_BGRA or GL_RGBA).
    bool processFrame(unsigned char* pixelData, unsigned int width, unsigned int height, int GLformat);

    // Clear any pending commands without executing them.
    void clearQueue();

    // Check whether there are pending (queued) commands.
    bool hasPendingCommands() const;

    // Access the current pending queue (read-only).
    const std::vector<std::string>& pendingCommands() const;

private:
    bool m_enabled = false;
    char m_delimiter = ';';
    std::vector<std::string> m_operationsQueue;
    CommandCallback m_callback;
    QRCodeReader* m_reader = nullptr;

    QRCommand parseCommand(const std::string& raw) const;
};

#endif // QRCOMMANDPROCESSOR_H
