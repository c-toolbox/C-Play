/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LOGFILEWRITER_H
#define LOGFILEWRITER_H

#include <string>

/**
 * Fans out sgct::Log messages to one or more log files.
 *
 * sgct::Log supports a single message callback, so this utility owns that slot: the first
 * open() call registers a callback that appends every logged message to all currently open
 * files, and the last close()/closeAll() removes it again. Multiple consumers (e.g. the
 * command-line --logfile option and the general logging setting) can therefore write to
 * different files at the same time without overwriting each other's callback.
 */
namespace LogFileWriter {

/**
 * Opens the log file at path, creating parent directories if needed, and starts writing all
 * subsequent sgct::Log messages into it. If truncate is true any existing content is
 * replaced; otherwise new messages are appended to the existing file. Returns an id for
 * close(), or -1 if the file could not be opened.
 */
int open(const std::string &path, bool truncate = false);

/**
 * Stops writing to the file previously opened with the given id and closes it. Unknown ids
 * are ignored. When this was the last open file, the sgct::Log callback is removed again.
 */
void close(int id);

/**
 * Closes all open log files (used at shutdown). Safe to call when nothing is open.
 */
void closeAll();

} // namespace LogFileWriter

#endif // LOGFILEWRITER_H
