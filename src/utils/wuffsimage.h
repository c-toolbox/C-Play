/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WUFFSIMAGE_H
#define WUFFSIMAGE_H

#include <filesystem>
#include <string>
#include <vector>

namespace WuffsImage {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;
};

bool isSupportedExtension(const std::filesystem::path &path);
bool decodeRgbaFile(const std::filesystem::path &path, Image &image, std::string *errorMessage = nullptr);

} // namespace WuffsImage

#endif // WUFFSIMAGE_H