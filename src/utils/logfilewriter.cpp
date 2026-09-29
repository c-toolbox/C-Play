/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "utils/logfilewriter.h"

#include <sgct/log.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string_view>

namespace {

std::mutex g_logFilesMutex;
std::map<int, std::ofstream> g_logFiles;
int g_nextLogFileId = 0;

// Invoked by sgct::Log for every message that passes the notify level. It can fire from any
// thread (SGCT network threads as well as the main thread), so the file map is guarded by
// its own mutex - Log does not hold a lock while invoking the callback.
void logFileCallback(sgct::Log::Level, std::string_view message) {
    std::lock_guard<std::mutex> lock(g_logFilesMutex);
    for (auto &entry : g_logFiles) {
        entry.second << message << '\n';
    }
}

} // namespace

namespace LogFileWriter {

int open(const std::string &path, bool truncate) {
    const std::filesystem::path filePath(path);
    if (filePath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(filePath.parent_path(), ec); // ignore: open() reports failure
    }

    std::ofstream file(path, truncate ? (std::ofstream::out | std::ofstream::trunc)
                                      : (std::ofstream::out | std::ofstream::app));
    if (!file.is_open()) {
        return -1;
    }

    std::lock_guard<std::mutex> lock(g_logFilesMutex);
    const int id = g_nextLogFileId++;
    g_logFiles.emplace(id, std::move(file));
    if (g_logFiles.size() == 1) {
        // First file: route all sgct::Log messages into the open files. The timestamp and
        // level prefix are enabled for the console as well; like the command-line --logfile
        // option this is a one-way switch that stays on for the rest of the session.
        sgct::Log::instance().setShowTime(true);
        sgct::Log::instance().setShowLogLevel(true);
        sgct::Log::instance().setLogCallback(logFileCallback);
    }
    return id;
}

void close(int id) {
    std::lock_guard<std::mutex> lock(g_logFilesMutex);
    g_logFiles.erase(id);
    if (g_logFiles.empty()) {
        sgct::Log::instance().setLogCallback(nullptr);
    }
}

void closeAll() {
    std::lock_guard<std::mutex> lock(g_logFilesMutex);
    g_logFiles.clear();
    sgct::Log::instance().setLogCallback(nullptr);
}

} // namespace LogFileWriter
