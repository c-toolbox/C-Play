/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "wuffsimage.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__STATIC_FUNCTIONS
#define WUFFS_CONFIG__DISABLE_MSVC_CPU_ARCH__X86_64_FAMILY
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__ADLER32
#define WUFFS_CONFIG__MODULE__BMP
#define WUFFS_CONFIG__MODULE__CRC32
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__JPEG
#define WUFFS_CONFIG__MODULE__PNG
#define WUFFS_CONFIG__MODULE__TARGA
#define WUFFS_CONFIG__MODULE__ZLIB
#define WUFFS_CONFIG__DST_PIXEL_FORMAT__ENABLE_ALLOWLIST
#define WUFFS_CONFIG__DST_PIXEL_FORMAT__ALLOW_RGBA_NONPREMUL
#define WUFFS_CONFIG__ENABLE_DROP_IN_REPLACEMENT__STB
#include <wuffs-unsupported-snapshot.c>

namespace {

std::string lowerExtension(const std::filesystem::path &path)
{
    std::string extension = path.extension().generic_string();
    std::ranges::transform(extension, extension.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return extension;
}

bool readFile(const std::filesystem::path &path, std::vector<unsigned char> &bytes, std::string *errorMessage)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        if (errorMessage) {
            *errorMessage = "Could not open image file '" + path.string() + "'";
        }
        return false;
    }

    const std::streamsize size = file.tellg();
    if (size <= 0 || size > static_cast<std::streamsize>(std::numeric_limits<int>::max())) {
        if (errorMessage) {
            *errorMessage = "Image file is empty or too large for Wuffs loading: '" + path.string() + "'";
        }
        return false;
    }

    bytes.resize(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char *>(bytes.data()), size)) {
        if (errorMessage) {
            *errorMessage = "Could not read image file '" + path.string() + "'";
        }
        return false;
    }
    return true;
}

} // namespace

namespace WuffsImage {

bool isSupportedExtension(const std::filesystem::path &path)
{
    const std::string extension = lowerExtension(path);
    return extension == ".bmp" ||
        extension == ".jpg" ||
        extension == ".jpeg" ||
        extension == ".png" ||
        extension == ".tga";
}

bool decodeRgbaFile(const std::filesystem::path &path, Image &image, std::string *errorMessage)
{
    std::vector<unsigned char> bytes;
    if (!readFile(path, bytes, errorMessage)) {
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char *decoded = stbi_load_from_memory(
        bytes.data(),
        static_cast<int>(bytes.size()),
        &width,
        &height,
        &channels,
        STBI_rgb_alpha);

    if (!decoded || width <= 0 || height <= 0) {
        if (decoded) {
            stbi_image_free(decoded);
        }
        if (errorMessage) {
            *errorMessage = "Wuffs could not decode image file '" + path.string() + "'";
        }
        return false;
    }

    const std::size_t pixelBytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    image.width = width;
    image.height = height;
    image.rgba.assign(decoded, decoded + pixelBytes);
    stbi_image_free(decoded);
    return true;
}

} // namespace WuffsImage