/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NODESTREAMGLCONTEXT_H
#define NODESTREAMGLCONTEXT_H

// Hidden OpenGL context that shares objects with the context current at create(),
// so that a worker thread can wait on fences of that context. Windows (WGL) only.
class NodeStreamSharedContext {
public:
    NodeStreamSharedContext() = default;
    ~NodeStreamSharedContext();

    NodeStreamSharedContext(const NodeStreamSharedContext &) = delete;
    NodeStreamSharedContext &operator=(const NodeStreamSharedContext &) = delete;

    // Must be called on the thread with the context to share current.
    bool create();
    // Must be called on the thread that called create(), with the context not current anywhere.
    void destroy();
    bool isValid() const;

    bool makeCurrent();
    void doneCurrent();

private:
    void *m_window = nullptr;
    void *m_dc = nullptr;
    void *m_context = nullptr;
};

#endif // NODESTREAMGLCONTEXT_H
