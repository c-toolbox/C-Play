/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nodestreamglcontext.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>

namespace {

constexpr GLenum kGlMajorVersion = 0x821B;
constexpr GLenum kGlMinorVersion = 0x821C;
constexpr GLenum kGlContextProfileMask = 0x9126;
constexpr int kWglContextMajorVersion = 0x2091;
constexpr int kWglContextMinorVersion = 0x2092;
constexpr int kWglContextProfileMask = 0x9126;
constexpr const wchar_t *kWindowClass = L"CPlayNodeStreamGL";

using CreateContextAttribsFn = HGLRC(WINAPI *)(HDC, HGLRC, const int *);

bool registerWindowClass() {
    static const bool registered = [] {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kWindowClass;
        return RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }();
    return registered;
}

} // namespace

NodeStreamSharedContext::~NodeStreamSharedContext() {
    destroy();
}

bool NodeStreamSharedContext::create() {
    destroy();

    const HGLRC shareContext = wglGetCurrentContext();
    const HDC shareDc = wglGetCurrentDC();
    if (!shareContext || !shareDc || !registerWindowClass())
        return false;
    const auto createContextAttribs =
        reinterpret_cast<CreateContextAttribsFn>(wglGetProcAddress("wglCreateContextAttribsARB"));
    if (!createContextAttribs)
        return false;

    const int pixelFormat = GetPixelFormat(shareDc);
    PIXELFORMATDESCRIPTOR pfd{};
    if (pixelFormat == 0 || DescribePixelFormat(shareDc, pixelFormat, sizeof(pfd), &pfd) == 0)
        return false;

    const HWND window = CreateWindowExW(0, kWindowClass, L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
                                        GetModuleHandleW(nullptr), nullptr);
    if (!window)
        return false;
    m_window = window;
    const HDC dc = GetDC(window);
    m_dc = dc;
    if (!dc || !SetPixelFormat(dc, pixelFormat, &pfd)) {
        destroy();
        return false;
    }

    GLint major = 0;
    GLint minor = 0;
    GLint profile = 0;
    glGetIntegerv(kGlMajorVersion, &major);
    glGetIntegerv(kGlMinorVersion, &minor);
    glGetIntegerv(kGlContextProfileMask, &profile);
    const int attributes[] = {kWglContextMajorVersion, major, kWglContextMinorVersion, minor,
                              kWglContextProfileMask, profile, 0};
    m_context = createContextAttribs(dc, shareContext, attributes);
    if (!m_context) {
        destroy();
        return false;
    }
    return true;
}

void NodeStreamSharedContext::destroy() {
    if (m_context) {
        wglDeleteContext(static_cast<HGLRC>(m_context));
        m_context = nullptr;
    }
    if (m_window) {
        if (m_dc)
            ReleaseDC(static_cast<HWND>(m_window), static_cast<HDC>(m_dc));
        DestroyWindow(static_cast<HWND>(m_window));
    }
    m_window = nullptr;
    m_dc = nullptr;
}

bool NodeStreamSharedContext::isValid() const {
    return m_context != nullptr;
}

bool NodeStreamSharedContext::makeCurrent() {
    return m_context && wglMakeCurrent(static_cast<HDC>(m_dc), static_cast<HGLRC>(m_context));
}

void NodeStreamSharedContext::doneCurrent() {
    if (m_context)
        wglMakeCurrent(nullptr, nullptr);
}

#else

NodeStreamSharedContext::~NodeStreamSharedContext() = default;

bool NodeStreamSharedContext::create() {
    return false;
}

void NodeStreamSharedContext::destroy() {
}

bool NodeStreamSharedContext::isValid() const {
    return false;
}

bool NodeStreamSharedContext::makeCurrent() {
    return false;
}

void NodeStreamSharedContext::doneCurrent() {
}

#endif
